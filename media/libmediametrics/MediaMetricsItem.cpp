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

#define LOG_TAG "mediametrics::Item"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <mutex>
#include <set>
#include <unordered_map>

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

#define DEBUG_SERVICEACCESS     0
#define DEBUG_API               0
#define DEBUG_ALLOCATIONS       0

// after this many failed attempts, we stop trying [from this process] and just say that
// the service is off.
#define SVC_TRIES               2

static const std::unordered_map<std::string, int32_t>& getErrorStringMap() {
    // DO NOT MODIFY VALUES (OK to add new ones).
    // This may be found in frameworks/av/media/libmediametrics/include/MediaMetricsConstants.h
    static std::unordered_map<std::string, int32_t> map{
        {"",                                      NO_ERROR},
        {AMEDIAMETRICS_PROP_STATUS_VALUE_OK,       NO_ERROR},
        {AMEDIAMETRICS_PROP_STATUS_VALUE_ARGUMENT, BAD_VALUE},
        {AMEDIAMETRICS_PROP_STATUS_VALUE_IO,       DEAD_OBJECT},
        {AMEDIAMETRICS_PROP_STATUS_VALUE_MEMORY,   NO_MEMORY},
        {AMEDIAMETRICS_PROP_STATUS_VALUE_SECURITY, PERMISSION_DENIED},
        {AMEDIAMETRICS_PROP_STATUS_VALUE_STATE,    INVALID_OPERATION},
        {AMEDIAMETRICS_PROP_STATUS_VALUE_TIMEOUT,  WOULD_BLOCK},
        {AMEDIAMETRICS_PROP_STATUS_VALUE_UNKNOWN,  UNKNOWN_ERROR},
    };
    return map;
}

status_t statusStringToStatus(const char *error) {
    const auto& map = getErrorStringMap();
    if (error == nullptr || error[0] == '\0') return NO_ERROR;
    auto it = map.find(error);
    if (it != map.end()) {
        return it->second;
    }
    return UNKNOWN_ERROR;
}

mediametrics::Item* mediametrics::Item::convert(mediametrics_handle_t handle) {
    mediametrics::Item *item = (android::mediametrics::Item *) handle;
    return item;
}

mediametrics_handle_t mediametrics::Item::convert(mediametrics::Item *item ) {
    mediametrics_handle_t handle = (mediametrics_handle_t) item;
    return handle;
}

mediametrics::Item::~Item() {
    if (DEBUG_ALLOCATIONS) {
        ALOGD("Destroy  mediametrics::Item @ %p", this);
    }
}

mediametrics::Item &mediametrics::Item::setTimestamp(nsecs_t ts) {
    mTimestamp = ts;
    return *this;
}

nsecs_t mediametrics::Item::getTimestamp() const {
    return mTimestamp;
}

mediametrics::Item &mediametrics::Item::setPid(pid_t pid) {
    mPid = pid;
    return *this;
}

pid_t mediametrics::Item::getPid() const {
    return mPid;
}

mediametrics::Item &mediametrics::Item::setUid(uid_t uid) {
    mUid = uid;
    return *this;
}

uid_t mediametrics::Item::getUid() const {
    return mUid;
}

mediametrics::Item &mediametrics::Item::setPkgName(const std::string &pkgName) {
    mPkgName = pkgName;
    return *this;
}

mediametrics::Item &mediametrics::Item::setPkgVersionCode(int64_t pkgVersionCode) {
    mPkgVersionCode = pkgVersionCode;
    return *this;
}

int64_t mediametrics::Item::getPkgVersionCode() const {
    return mPkgVersionCode;
}

// remove indicated keys and their values
// return value is # keys removed
size_t mediametrics::Item::filter(size_t n, const char *attrs[]) {
    size_t zapped = 0;
    for (size_t i = 0; i < n; ++i) {
        zapped += mProps.erase(attrs[i]);
    }
    return zapped;
}

// remove any keys NOT in the provided list
// return value is # keys removed
size_t mediametrics::Item::filterNot(size_t n, const char *attrs[]) {
    std::set<std::string> check(attrs, attrs + n);
    size_t zapped = 0;
    for (auto it = mProps.begin(); it != mProps.end();) {
        if (check.find(it->first) != check.end()) {
            ++it;
        } else {
           it = mProps.erase(it);
           ++zapped;
        }
    }
    return zapped;
}

const char *mediametrics::Item::toCString() {
    std::string val = toString();
    return strdup(val.c_str());
}

/*
 * Similar to audio_utils/clock.h but customized for displaying mediametrics time.
 */

void nsToString(int64_t ns, char *buffer, size_t bufferSize, PrintFormat format)
{
    if (bufferSize == 0) return;

    const int one_second = 1000000000;
    const time_t sec = ns / one_second;
    struct tm tm;

    // Supported on bionic, glibc, and macOS, but not mingw.
    if (localtime_r(&sec, &tm) == NULL) {
        buffer[0] = '\0';
        return;
    }

    switch (format) {
    default:
    case kPrintFormatLong:
        if (snprintf(buffer, bufferSize, "%02d-%02d %02d:%02d:%02d.%03d",
            tm.tm_mon + 1, // localtime_r uses months in 0 - 11 range
            tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
            (int)(ns % one_second / 1000000)) < 0) {
            buffer[0] = '\0'; // null terminate on format error, which should not happen
        }
        break;
    case kPrintFormatShort:
        if (snprintf(buffer, bufferSize, "%02d:%02d:%02d.%03d",
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            (int)(ns % one_second / 1000000)) < 0) {
            buffer[0] = '\0'; // null terminate on format error, which should not happen
        }
        break;
    }
}

std::string mediametrics::Item::toString() const {
    std::string result;
    char buffer[kMaxPropertyStringSize];

    snprintf(buffer, sizeof(buffer), "{%s, (%s), (%s, %d, %d)",
            mKey.c_str(),
            timeStringFromNs(mTimestamp, kPrintFormatLong).time,
            mPkgName.c_str(), mPid, mUid
           );
    result.append(buffer);
    bool first = true;
    for (auto &prop : *this) {
        prop.toStringBuffer(buffer, sizeof(buffer));
        result += first ? ", (" : ", ";
        result += buffer;
        first = false;
    }
    result.append(")}");
    return result;
}

// for the lazy, we offer methods that finds the service and
// calls the appropriate daemon
bool mediametrics::Item::selfrecord() {
    ALOGD_IF(DEBUG_API, "%s: delivering %s", __func__, this->toString().c_str());

    char *str;
    size_t size;
    status_t status = writeToByteString(&str, &size);
    if (status == NO_ERROR) {
        status = submitBuffer(str, size);
        free(str);
    }
    if (status != NO_ERROR) {
        ALOGW("%s: failed to record: %s", __func__, this->toString().c_str());
        return false;
    }
    return true;
}

//static
bool BaseItem::isEnabled() {
    // completely skip logging from certain UIDs. We do this here
    // to avoid the multi-second timeouts while we learn that
    // sepolicy will not let us find the service.
    // We do this only for a select set of UIDs
    // The sepolicy protection is still in place, we just want a faster
    // response from this specific, small set of uids.

    // This is checked only once in the lifetime of the process.
    const uid_t uid = getuid();
    const uid_t appid = multiuser_get_app_id(uid);

    if (appid == AID_RADIO) {
        // telephony subsystem, RIL
        return false;
    }

    if (appid >= AID_ISOLATED_START && appid <= AID_ISOLATED_END) {
        // Some isolated processes can access the audio system; see
        // AudioSystem::setAudioFlingerBinder (currently only the HotwordDetectionService). Instead
        // of also allowing access to the MediaMetrics service, it's simpler to just disable it for
        // now.
        // TODO(b/190151205): Either allow the HotwordDetectionService to access MediaMetrics or
        // make this disabling specific to that process.
        return false;
    }

    int enabled = property_get_int32(Item::EnabledProperty, -1);
    if (enabled == -1) {
        enabled = property_get_int32(Item::EnabledPropertyPersist, -1);
    }
    if (enabled == -1) {
        enabled = Item::EnabledProperty_default;
    }
    return enabled > 0;
}

// monitor health of our connection to the metrics service
class MediaMetricsDeathNotifier : public IBinder::DeathRecipient {
        virtual void binderDied(const wp<IBinder> &) {
            ALOGW("Reacquire service connection on next request");
            BaseItem::dropInstance();
        }
};

static sp<MediaMetricsDeathNotifier> sNotifier;
static sp<media::IMediaMetricsService> sMediaMetricsService;
static std::mutex sServiceMutex;
static int sRemainingBindAttempts = SVC_TRIES;

// moving this out of the class removes all service references from <MediaMetricsItem.h>
// and simplifies moving things to a module
static
sp<media::IMediaMetricsService> getService() {
    static const char *servicename = "media.metrics";
    static const bool enabled = BaseItem::isEnabled(); // singleton initialized

    if (enabled == false) {
        ALOGD_IF(DEBUG_SERVICEACCESS, "disabled");
        return nullptr;
    }
    std::lock_guard _l(sServiceMutex);
    // think of remainingBindAttempts as telling us whether service == nullptr because
    // (1) we haven't tried to initialize it yet
    // (2) we've tried to initialize it, but failed.
    if (sMediaMetricsService == nullptr && sRemainingBindAttempts > 0) {
        const char *badness = "";
        sp<IServiceManager> sm = defaultServiceManager();
        if (sm != nullptr) {
            sp<IBinder> binder = sm->getService(String16(servicename));
            if (binder != nullptr) {
                sMediaMetricsService = interface_cast<media::IMediaMetricsService>(binder);
                sNotifier = new MediaMetricsDeathNotifier();
                binder->linkToDeath(sNotifier);
            } else {
                badness = "did not find service";
            }
        } else {
            badness = "No Service Manager access";
        }
        if (sMediaMetricsService == nullptr) {
            if (sRemainingBindAttempts > 0) {
                sRemainingBindAttempts--;
            }
            ALOGD_IF(DEBUG_SERVICEACCESS, "%s: unable to bind to service %s: %s",
                    __func__, servicename, badness);
        }
    }
    return sMediaMetricsService;
}

// static
void BaseItem::dropInstance() {
    std::lock_guard  _l(sServiceMutex);
    sRemainingBindAttempts = SVC_TRIES;
    sMediaMetricsService = nullptr;
}

// static
status_t BaseItem::submitBuffer(const char *buffer, size_t size) {
    ALOGD_IF(DEBUG_API, "%s: delivering %zu bytes", __func__, size);

    // Validate size
    if (size > std::numeric_limits<int32_t>::max()) return BAD_VALUE;

    // Do we have the service available?
    sp<media::IMediaMetricsService> svc = getService();
    if (svc == nullptr)  return NO_INIT;

    ::android::status_t status = NO_ERROR;
    if constexpr (/* DISABLES CODE */ (false)) {
        // THIS PATH IS FOR REFERENCE ONLY.
        // It is compiled so that any changes to IMediaMetricsService::submitBuffer()
        // will lead here.  If this code is changed, the else branch must
        // be changed as well.
        //
        // Use the AIDL calling interface - this is a bit slower as a byte vector must be
        // constructed. As the call is one-way, the only a transaction error occurs.
        status = svc->submitBuffer({buffer, buffer + size}).transactionError();
    } else {
        // Use the Binder calling interface - this direct implementation avoids
        // malloc/copy/free for the vector and reduces the overhead for logging.
        // We based this off of the AIDL generated file:
        // out/soong/.intermediates/frameworks/av/media/libmediametrics/mediametricsservice-aidl-unstable-cpp-source/gen/android/media/IMediaMetricsService.cpp
        // TODO: Create an AIDL C++ back end optimized form of vector writing.
        ::android::Parcel _aidl_data;
        ::android::Parcel _aidl_reply; // we don't care about this as it is one-way.

        status = _aidl_data.writeInterfaceToken(svc->getInterfaceDescriptor());
        if (status != ::android::OK) goto _aidl_error;

        status = _aidl_data.writeInt32(static_cast<int32_t>(size));
        if (status != ::android::OK) goto _aidl_error;

        status = _aidl_data.write(buffer, static_cast<int32_t>(size));
        if (status != ::android::OK) goto _aidl_error;

        status = ::android::IInterface::asBinder(svc)->transact(
                ::android::media::BnMediaMetricsService::TRANSACTION_submitBuffer,
                _aidl_data, &_aidl_reply, ::android::IBinder::FLAG_ONEWAY);

        // AIDL permits setting a default implementation for additional functionality.
        // See go/aog/713984. This is not used here.
        // if (status == ::android::UNKNOWN_TRANSACTION
        //         && ::android::media::IMediaMetricsService::getDefaultImpl()) {
        //     status = ::android::media::IMediaMetricsService::getDefaultImpl()
        //             ->submitBuffer(immutableByteVectorFromBuffer(buffer, size))
        //             .transactionError();
        // }
    }

    if (status == NO_ERROR) return NO_ERROR;

    _aidl_error:
    ALOGW("%s: failed(%d) to record: %zu bytes", __func__, status, size);
    return status;
}

} // namespace android::mediametrics
