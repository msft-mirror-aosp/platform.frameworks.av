/*
 * Copyright (C) 2024 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "NdkMediaCodecStore"

#include "NdkMediaCodecInfoPriv.h"

#include <media/NdkMediaCodecStore.h>
#include <media/NdkMediaFormatPriv.h>

#include <media/IMediaCodecList.h>

#include <media/MediaCodecInfo.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaCodecConstants.h>
#include <media/stagefright/MediaCodecList.h>

using namespace android;

static sp<IMediaCodecList> sCodecList;
static std::vector<AMediaCodecSupportedMediaType> sMediaTypes;
static std::vector<AMediaCodecInfo> sCodecInfos;

static std::map<std::string, AMediaCodecInfo> sNameToInfoMap;
static std::map<std::string, std::vector<AMediaCodecInfo>> sTypeToInfoList;

static void initMediaTypes() {
    if (sCodecList == nullptr) {
        sCodecList = MediaCodecList::getInstance();
    }

    std::map<std::string, AMediaCodecSupportedMediaType> typesInfoMap;
    std::vector<std::string> mediaTypes; // Keep the order of media types appearing in sCodecList.
    for (size_t idx = 0; idx < sCodecList->countCodecs(); idx++) {
        sp<MediaCodecInfo> codecInfo = sCodecList->getCodecInfo(idx);
        if (codecInfo == nullptr) {
            ALOGW("NULL MediaCodecInfo in MediaCodecList");
            continue;
        }
        Vector<AString> codecMediaTypes;
        codecInfo->getSupportedMediaTypes(&codecMediaTypes);
        for (AString codecMediaType : codecMediaTypes) {
            std::string mediaType = std::string(codecMediaType.c_str());

            // Excludes special codecs from NDK
            const std::shared_ptr<CodecCapabilities> codecCaps
                    = codecInfo->getCodecCapsFor(mediaType.c_str());
            if (codecCaps->isFeatureSupported(FEATURE_SpecialCodec)) {
                continue;
            }

            auto it = typesInfoMap.find(mediaType);
            if (it == typesInfoMap.end()) {
                char *mediaTypePtr = new char[mediaType.size()+1];
                strncpy(mediaTypePtr, mediaType.c_str(), mediaType.size()+1);
                it = typesInfoMap.emplace(mediaType,
                        (AMediaCodecSupportedMediaType) { mediaTypePtr, 0 }).first;
                mediaTypes.push_back(mediaType);
            }
            uint32_t &mode = it->second.mMode;
            mode |= (codecInfo->isEncoder() ? AMediaCodecSupportedMediaType::FLAG_ENCODER
                                            : AMediaCodecSupportedMediaType::FLAG_DECODER);
        }
    }

    // sMediaTypes keeps the order of media types appearing in sCodecList.
    for (std::string &type : mediaTypes) {
        sMediaTypes.push_back(typesInfoMap.find(type)->second);
    }
}

static void initCodecInfoMap() {
    if (sCodecList == nullptr) {
        sCodecList = MediaCodecList::getInstance();
    }

    for (size_t idx = 0; idx < sCodecList->countCodecs(); idx++) {
        sp<MediaCodecInfo> codecInfo = sCodecList->getCodecInfo(idx);
        if (codecInfo == nullptr) {
            ALOGW("NULL MediaCodecInfo in MediaCodecList");
            continue;
        }

        Vector<AString> codecMediaTypes;
        codecInfo->getSupportedMediaTypes(&codecMediaTypes);
        bool useTypeSuffix = codecMediaTypes.size() > 1;
        for (AString codecMediaType : codecMediaTypes) {
            std::string mediaType = std::string(codecMediaType.c_str());

            // Excludes special codecs from NDK
            const std::shared_ptr<CodecCapabilities> codecCaps
                    = codecInfo->getCodecCapsFor(mediaType.c_str());
            if (codecCaps->isFeatureSupported(FEATURE_SpecialCodec)) {
                continue;
            }

            // get the type name after the slash. e.g. video/x.on2.vp8
            size_t slashIx = mediaType.find_last_of('/');
            if (slashIx == std::string::npos) {
                slashIx = 0;
            } else {
                slashIx++;
            }
            std::string ndkBaseName = std::string(codecInfo->getCodecName());
            if (useTypeSuffix) {
                // If there are multiple supported media types,
                // add the type to the end of the name to disambiguate names.
                ndkBaseName += "." + mediaType.substr(slashIx);
            }

            int32_t copyIx = 0;
            std::string ndkName;
            // if a name is already registered,
            // add ".1", ".2", ... at the end to disambiguate names.
            while (true) {
                ndkName = ndkBaseName;
                if (copyIx > 0) {
                    ndkName += "." + std::to_string(copyIx);
                }
                if (!sNameToInfoMap.contains(ndkName)) {
                    break;
                }
                copyIx++;
            }

            AMediaCodecInfo info = AMediaCodecInfo(ndkName, codecInfo, codecCaps, mediaType);
            sCodecInfos.push_back(info);
            sNameToInfoMap.emplace(ndkName, info);

            auto it = sTypeToInfoList.find(mediaType);
            if (it == sTypeToInfoList.end()) {
                std::vector<AMediaCodecInfo> infoList;
                infoList.push_back(info);
                sTypeToInfoList.emplace(mediaType, infoList);
            } else {
                it->second.push_back(info);
            }
        }
    }
}

static bool codecHandlesFormat(const AMediaCodecInfo &codecInfo,
        sp<AMessage> format, bool isEncoder) {
    return codecInfo.mCodecCaps->isEncoder() == isEncoder
            && codecInfo.mCodecCaps->isFormatSupported(format);
}

static media_status_t findNextCodecForFormat(
        const AMediaFormat *format, bool isEncoder, const AMediaCodecInfo **outCodecInfo) {
    if (outCodecInfo == nullptr) {
        return AMEDIA_ERROR_INVALID_PARAMETER;
    }

    if (sCodecInfos.empty()) {
        initCodecInfoMap();
    }

    std::vector<AMediaCodecInfo> *infos;
    sp<AMessage> nativeFormat;
    if (format == nullptr) {
        infos = &sCodecInfos;
    } else {
        AMediaFormat_getFormat(format, &nativeFormat);
        AString mime;
        if (!nativeFormat->findString(KEY_MIME, &mime)) {
            return AMEDIA_ERROR_INVALID_PARAMETER;
        }

        std::string mediaType = std::string(mime.c_str());
        std::map<std::string, std::vector<AMediaCodecInfo>>::iterator it
                = sTypeToInfoList.find(mediaType);
        if (it == sTypeToInfoList.end()) {
            return AMEDIA_ERROR_UNSUPPORTED;
        }
        infos = &(it->second);
    }

    bool found = *outCodecInfo == nullptr;
    for (const AMediaCodecInfo &info : *infos) {
        if (found && (format == nullptr
                || codecHandlesFormat(info, nativeFormat, isEncoder))) {
            *outCodecInfo = &info;
            return AMEDIA_OK;
        }
        if (*outCodecInfo == &info) {
            found = true;
        }
    }
    *outCodecInfo = nullptr;
    return AMEDIA_ERROR_UNSUPPORTED;
}

extern "C" {

EXPORT
media_status_t AMediaCodecStore_getSupportedMediaTypes(
        const AMediaCodecSupportedMediaType **outMediaTypes, size_t *outCount) {
    if (outMediaTypes == nullptr) {
        return AMEDIA_ERROR_INVALID_PARAMETER;
    }

    if (sMediaTypes.empty()) {
        initMediaTypes();
    }

    *outCount = sMediaTypes.size();
    *outMediaTypes = sMediaTypes.data();

    return AMEDIA_OK;
}

EXPORT
media_status_t AMediaCodecStore_findNextDecoderForFormat(
        const AMediaFormat *format, const AMediaCodecInfo **outCodecInfo){
    return findNextCodecForFormat(format, false, outCodecInfo);
}

EXPORT
media_status_t AMediaCodecStore_findNextEncoderForFormat(
        const AMediaFormat *format, const AMediaCodecInfo **outCodecInfo){
    return findNextCodecForFormat(format, true, outCodecInfo);
}

EXPORT
media_status_t AMediaCodecStore_getCodecInfo(
        const char *name, const AMediaCodecInfo **outCodecInfo) {
    if (outCodecInfo == nullptr || name == nullptr) {
        return AMEDIA_ERROR_INVALID_PARAMETER;
    }

    if (sNameToInfoMap.empty()) {
        initCodecInfoMap();
    }

    auto it = sNameToInfoMap.find(std::string(name));
    if (it == sNameToInfoMap.end()) {
        *outCodecInfo = nullptr;
        return AMEDIA_ERROR_UNSUPPORTED;
    } else {
        *outCodecInfo = &(it->second);
        return AMEDIA_OK;
    }
}

}