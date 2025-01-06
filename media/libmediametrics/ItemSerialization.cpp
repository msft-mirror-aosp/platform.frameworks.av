/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "mediametrics::Item-Serialization"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/types.h>

#include <binder/Parcel.h>
#include <cutils/multiuser.h>
#include <cutils/properties.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/SortedVector.h>
#include <utils/threads.h>

#include <android/media/BnMediaMetricsService.h> // for direct Binder access
#include <android/media/IMediaMetricsService.h>
#include <binder/IServiceManager.h>
#include <media/MediaMetricsItem.h>
#include <private/android_filesystem_config.h>

// Max per-property string size before truncation in toString().
// Do not make too large, as this is used for dumpsys purposes.
static constexpr size_t kMaxPropertyStringSize = 4096;

namespace android::mediametrics {

// Parcel / serialize things for binder calls
//

status_t mediametrics::Item::readFromParcel(const Parcel& data) {
    int32_t version;
    status_t status = data.readInt32(&version);
    if (status != NO_ERROR) return status;

    switch (version) {
    case 0:
      return readFromParcel0(data);
    default:
      ALOGE("%s: unsupported parcel version: %d", __func__, version);
      return INVALID_OPERATION;
    }
}

status_t mediametrics::Item::readFromParcel0(const Parcel& data) {
    const char *s = data.readCString();
    mKey = s == nullptr ? "" : s;
    int32_t pid, uid;
    status_t status = data.readInt32(&pid) ?: data.readInt32(&uid);
    if (status != NO_ERROR) return status;
    mPid = (pid_t)pid;
    mUid = (uid_t)uid;
    s = data.readCString();
    mPkgName = s == nullptr ? "" : s;
    int32_t count;
    int64_t version, timestamp;
    status = data.readInt64(&version) ?: data.readInt64(&timestamp) ?: data.readInt32(&count);
    if (status != NO_ERROR) return status;
    if (count < 0) return BAD_VALUE;
    mPkgVersionCode = version;
    mTimestamp = timestamp;
    for (int i = 0; i < count; i++) {
        Prop prop;
        status_t status = prop.readFromParcel(data);
        if (status != NO_ERROR) return status;
        mProps[prop.getName()] = std::move(prop);
    }
    return NO_ERROR;
}

status_t mediametrics::Item::writeToParcel(Parcel *data) const {
    if (data == nullptr) return BAD_VALUE;

    const int32_t version = 0;
    status_t status = data->writeInt32(version);
    if (status != NO_ERROR) return status;

    switch (version) {
    case 0:
      return writeToParcel0(data);
    default:
      ALOGE("%s: unsupported parcel version: %d", __func__, version);
      return INVALID_OPERATION;
    }
}

status_t mediametrics::Item::writeToParcel0(Parcel *data) const {
    status_t status =
        data->writeCString(mKey.c_str())
        ?: data->writeInt32(mPid)
        ?: data->writeInt32(mUid)
        ?: data->writeCString(mPkgName.c_str())
        ?: data->writeInt64(mPkgVersionCode)
        ?: data->writeInt64(mTimestamp);
    if (status != NO_ERROR) return status;

    data->writeInt32((int32_t)mProps.size());
    for (auto &prop : *this) {
        status = prop.writeToParcel(data);
        if (status != NO_ERROR) return status;
    }
    return NO_ERROR;
}

status_t mediametrics::Item::writeToByteString(char **pbuffer, size_t *plength) const
{
    if (pbuffer == nullptr || plength == nullptr)
        return BAD_VALUE;

    // get size
    const size_t keySizeZeroTerminated = strlen(mKey.c_str()) + 1;
    if (keySizeZeroTerminated > UINT16_MAX) {
        ALOGW("%s: key size %zu too large", __func__, keySizeZeroTerminated);
        return INVALID_OPERATION;
    }
    const uint16_t version = 0;
    const uint32_t header_size =
        sizeof(uint32_t)      // total size
        + sizeof(header_size) // header size
        + sizeof(version)     // encoding version
        + sizeof(uint16_t)    // key size
        + keySizeZeroTerminated // key, zero terminated
        + sizeof(int32_t)     // pid
        + sizeof(int32_t)     // uid
        + sizeof(int64_t)     // timestamp
        ;

    uint32_t size = header_size
        + sizeof(uint32_t) // # properties
        ;
    for (auto &prop : *this) {
        const size_t propSize = prop.getByteStringSize();
        if (propSize > UINT16_MAX) {
            ALOGW("%s: prop %s size %zu too large", __func__, prop.getName(), propSize);
            return INVALID_OPERATION;
        }
        if (__builtin_add_overflow(size, propSize, &size)) {
            ALOGW("%s: item size overflow at property %s", __func__, prop.getName());
            return INVALID_OPERATION;
        }
    }

    // since we fill every byte in the buffer (there is no padding),
    // malloc is used here instead of calloc.
    char * const build = (char *)malloc(size);
    if (build == nullptr) return NO_MEMORY;

    // we write in host byte-order; we think this is always little-endian
    // for the interesting devices (arm-based android, x86-based android).
    // we know the reader is running on the same host, so we expect the same
    // byte order on the consumption side.

    char *filling = build;
    char *buildmax = build + size;
    if (insert((uint32_t)size, &filling, buildmax) != NO_ERROR
            || insert(header_size, &filling, buildmax) != NO_ERROR
            || insert(version, &filling, buildmax) != NO_ERROR
            || insert((uint16_t)keySizeZeroTerminated, &filling, buildmax) != NO_ERROR
            || insert(mKey.c_str(), &filling, buildmax) != NO_ERROR
            || insert((int32_t)mPid, &filling, buildmax) != NO_ERROR
            || insert((int32_t)mUid, &filling, buildmax) != NO_ERROR
            || insert((int64_t)mTimestamp, &filling, buildmax) != NO_ERROR
            || insert((uint32_t)mProps.size(), &filling, buildmax) != NO_ERROR) {
        ALOGE("%s:could not write header", __func__);  // shouldn't happen
        free(build);
        return INVALID_OPERATION;
    }
    for (auto &prop : *this) {
        if (prop.writeToByteString(&filling, buildmax) != NO_ERROR) {
            free(build);
            // shouldn't happen
            ALOGE("%s:could not write prop %s", __func__, prop.getName());
            return INVALID_OPERATION;
        }
    }

    if (filling != buildmax) {
        ALOGE("%s: problems populating; wrote=%d planned=%d",
                __func__, (int)(filling - build), (int)size);
        free(build);
        return INVALID_OPERATION;
    }
    *pbuffer = build;
    *plength = size;
    return NO_ERROR;
}

status_t mediametrics::Item::readFromByteString(const char *bufferptr, size_t length)
{
    if (bufferptr == nullptr) return BAD_VALUE;

    // we read assuming host byte-order; we think this is always little-endian
    // for the interesting devices (arm-based android, x86-based android).
    // we know the writer is running on the same host,
    // and therefore should have this same byte order.

    const char *read = bufferptr;
    const char *readend = bufferptr + length;

    uint32_t size;
    uint32_t header_size;
    uint16_t version;
    uint16_t key_size;
    std::string key;
    int32_t pid;
    int32_t uid;
    int64_t timestamp;
    uint32_t propCount;
    if (extract(&size, &read, readend) != NO_ERROR
            || extract(&header_size, &read, readend) != NO_ERROR
            || extract(&version, &read, readend) != NO_ERROR
            || extract(&key_size, &read, readend) != NO_ERROR
            || extract(&key, &read, readend) != NO_ERROR
            || extract(&pid, &read, readend) != NO_ERROR
            || extract(&uid, &read, readend) != NO_ERROR
            || extract(&timestamp, &read, readend) != NO_ERROR
            || size > length
            || key.size() + 1 != key_size
            || header_size > size) {
        ALOGW("%s: invalid header", __func__);
        return INVALID_OPERATION;
    }
    mKey = std::move(key);
    const size_t pos = read - bufferptr;
    if (pos > header_size) {
        ALOGW("%s: invalid header pos:%zu > header_size:%u",
                __func__, pos, header_size);
        return INVALID_OPERATION;
    } else if (pos < header_size) {
        ALOGW("%s: mismatched header pos:%zu < header_size:%u, advancing",
                __func__, pos, header_size);
        read += (header_size - pos);
    }
    if (extract(&propCount, &read, readend) != NO_ERROR) {
        ALOGD("%s: cannot read prop count", __func__);
        return INVALID_OPERATION;
    }
    mPid = pid;
    mUid = uid;
    mTimestamp = timestamp;
    for (size_t i = 0; i < propCount; ++i) {
        Prop prop;
        if (prop.readFromByteString(&read, readend) != NO_ERROR) {
            ALOGW("%s: cannot read prop %zu", __func__, i);
            return INVALID_OPERATION;
        }
        mProps[prop.getName()] = std::move(prop);
    }
    return NO_ERROR;
}

status_t mediametrics::Item::Prop::readFromParcel(const Parcel& data)
{
    const char *key = data.readCString();
    if (key == nullptr) return BAD_VALUE;
    int32_t type;
    status_t status = data.readInt32(&type);
    if (status != NO_ERROR) return status;
    switch (type) {
    case mediametrics::kTypeInt32: {
        int32_t value;
        status = data.readInt32(&value);
        if (status != NO_ERROR) return status;
        mElem = value;
    } break;
    case mediametrics::kTypeInt64: {
        int64_t value;
        status = data.readInt64(&value);
        if (status != NO_ERROR) return status;
        mElem = value;
    } break;
    case mediametrics::kTypeDouble: {
        double value;
        status = data.readDouble(&value);
        if (status != NO_ERROR) return status;
        mElem = value;
    } break;
    case mediametrics::kTypeCString: {
        const char *s = data.readCString();
        if (s == nullptr) return BAD_VALUE;
        mElem = s;
    } break;
    case mediametrics::kTypeRate: {
        std::pair<int64_t, int64_t> rate;
        status = data.readInt64(&rate.first)
                ?: data.readInt64(&rate.second);
        if (status != NO_ERROR) return status;
        mElem = rate;
    } break;
    case mediametrics::kTypeNone: {
        mElem = std::monostate{};
    } break;
    default:
        ALOGE("%s: reading bad item type: %d", __func__, type);
        return BAD_VALUE;
    }
    setName(key);
    return NO_ERROR;
}

status_t mediametrics::Item::Prop::readFromByteString(
        const char **bufferpptr, const char *bufferptrmax)
{
    uint16_t len;
    std::string name;
    uint8_t type;
    status_t status = extract(&len, bufferpptr, bufferptrmax)
            ?: extract(&type, bufferpptr, bufferptrmax)
            ?: extract(&name, bufferpptr, bufferptrmax);
    if (status != NO_ERROR) return status;
    switch (type) {
    case mediametrics::kTypeInt32: {
        int32_t value;
        status = extract(&value, bufferpptr, bufferptrmax);
        if (status != NO_ERROR) return status;
        mElem = value;
    } break;
    case mediametrics::kTypeInt64: {
        int64_t value;
        status = extract(&value, bufferpptr, bufferptrmax);
        if (status != NO_ERROR) return status;
        mElem = value;
    } break;
    case mediametrics::kTypeDouble: {
        double value;
        status = extract(&value, bufferpptr, bufferptrmax);
        if (status != NO_ERROR) return status;
        mElem = value;
    } break;
    case mediametrics::kTypeRate: {
        std::pair<int64_t, int64_t> value;
        status = extract(&value.first, bufferpptr, bufferptrmax)
                ?: extract(&value.second, bufferpptr, bufferptrmax);
        if (status != NO_ERROR) return status;
        mElem = value;
    } break;
    case mediametrics::kTypeCString: {
        std::string value;
        status = extract(&value, bufferpptr, bufferptrmax);
        if (status != NO_ERROR) return status;
        mElem = std::move(value);
    } break;
    case mediametrics::kTypeNone: {
        mElem = std::monostate{};
    } break;
    default:
        ALOGE("%s: found bad prop type: %d, name %s",
                __func__, (int)type, mName.c_str());  // no payload sent
        return BAD_VALUE;
    }
    mName = name;
    return NO_ERROR;
}

} // namespace android::mediametrics
