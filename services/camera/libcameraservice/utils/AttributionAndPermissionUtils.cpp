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

#include "AttributionAndPermissionUtils.h"

#include <binder/AppOpsManager.h>
#include <binder/PermissionController.h>
#include <com_android_internal_camera_flags.h>
#include <cutils/properties.h>
#include <private/android_filesystem_config.h>

#include "CameraService.h"

#include <binder/IPCThreadState.h>
#include <hwbinder/IPCThreadState.h>
#include <binderthreadstate/CallerUtils.h>

namespace android {

namespace flags = com::android::internal::camera::flags;

const std::string AttributionAndPermissionUtils::sDumpPermission("android.permission.DUMP");
const std::string AttributionAndPermissionUtils::sManageCameraPermission(
        "android.permission.MANAGE_CAMERA");
const std::string AttributionAndPermissionUtils::sCameraPermission(
        "android.permission.CAMERA");
const std::string AttributionAndPermissionUtils::sSystemCameraPermission(
        "android.permission.SYSTEM_CAMERA");
const std::string AttributionAndPermissionUtils::sCameraHeadlessSystemUserPermission(
        "android.permission.CAMERA_HEADLESS_SYSTEM_USER");
const std::string AttributionAndPermissionUtils::sCameraPrivacyAllowlistPermission(
        "android.permission.CAMERA_PRIVACY_ALLOWLIST");
const std::string AttributionAndPermissionUtils::sCameraSendSystemEventsPermission(
        "android.permission.CAMERA_SEND_SYSTEM_EVENTS");
const std::string AttributionAndPermissionUtils::sCameraOpenCloseListenerPermission(
        "android.permission.CAMERA_OPEN_CLOSE_LISTENER");
const std::string AttributionAndPermissionUtils::sCameraInjectExternalCameraPermission(
        "android.permission.CAMERA_INJECT_EXTERNAL_CAMERA");

int AttributionAndPermissionUtils::getCallingUid() {
    if (getCurrentServingCall() == BinderCallType::HWBINDER) {
        return hardware::IPCThreadState::self()->getCallingUid();
    }
    return IPCThreadState::self()->getCallingUid();
}

int AttributionAndPermissionUtils::getCallingPid() {
    if (getCurrentServingCall() == BinderCallType::HWBINDER) {
        return hardware::IPCThreadState::self()->getCallingPid();
    }
    return IPCThreadState::self()->getCallingPid();
}

int64_t AttributionAndPermissionUtils::clearCallingIdentity() {
    if (getCurrentServingCall() == BinderCallType::HWBINDER) {
        return hardware::IPCThreadState::self()->clearCallingIdentity();
    }
    return IPCThreadState::self()->clearCallingIdentity();
}

void AttributionAndPermissionUtils::restoreCallingIdentity(int64_t token) {
    if (getCurrentServingCall() == BinderCallType::HWBINDER) {
        hardware::IPCThreadState::self()->restoreCallingIdentity(token);
    } else {
        IPCThreadState::self()->restoreCallingIdentity(token);
    }
    return;
}

// TODO(362551824): Make USE_CALLING_UID more explicit with a scoped enum.
bool AttributionAndPermissionUtils::resolveClientUid(/*inout*/ int& clientUid) {
    int callingUid = getCallingUid();

    if (clientUid == hardware::ICameraService::USE_CALLING_UID) {
        clientUid = callingUid;
    } else if (!isTrustedCallingUid(callingUid)) {
        return false;
    }

    return true;
}

// TODO(362551824): Make USE_CALLING_UID more explicit with a scoped enum.
bool AttributionAndPermissionUtils::resolveClientPid(/*inout*/ int& clientPid) {
    int callingUid = getCallingUid();
    int callingPid = getCallingPid();

    if (clientPid == hardware::ICameraService::USE_CALLING_PID) {
        clientPid = callingPid;
    } else if (!isTrustedCallingUid(callingUid)) {
        return false;
    }

    return true;
}

bool AttributionAndPermissionUtils::checkAutomotivePrivilegedClient(const std::string &cameraId,
        const AttributionSourceState &attributionSource) {
    if (isAutomotivePrivilegedClient(attributionSource.uid)) {
        // If cameraId is empty, then it means that this check is not used for the
        // purpose of accessing a specific camera, hence grant permission just
        // based on uid to the automotive privileged client.
        if (cameraId.empty())
            return true;

        auto cameraService = mCameraService.promote();
        if (cameraService == nullptr) {
            ALOGE("%s: CameraService unavailable.", __FUNCTION__);
            return false;
        }

        // If this call is used for accessing a specific camera then cam_id must be provided.
        // In that case, only pre-grants the permission for accessing the exterior system only
        // camera.
        return cameraService->isAutomotiveExteriorSystemCamera(cameraId);
    }

    return false;
}

bool AttributionAndPermissionUtils::checkPermissionForPreflight(const std::string &cameraId,
        const std::string &permission, const AttributionSourceState &attributionSource,
        const std::string& message, int32_t attributedOpCode) {
    if (checkAutomotivePrivilegedClient(cameraId, attributionSource)) {
        return true;
    }

    if (!flags::cache_permission_services()) {
        PermissionChecker permissionChecker;
        return permissionChecker.checkPermissionForPreflight(
                       toString16(permission), attributionSource, toString16(message),
                       attributedOpCode) != PermissionChecker::PERMISSION_HARD_DENIED;
    } else {
        return mPermissionChecker->checkPermissionForPreflight(
                       toString16(permission), attributionSource, toString16(message),
                       attributedOpCode) != PermissionChecker::PERMISSION_HARD_DENIED;
    }
}

// Can camera service trust the caller based on the calling UID?
bool AttributionAndPermissionUtils::isTrustedCallingUid(uid_t uid) {
    switch (uid) {
        case AID_MEDIA:        // mediaserver
        case AID_CAMERASERVER: // cameraserver
        case AID_RADIO:        // telephony
            return true;
        default:
            return false;
    }
}

bool AttributionAndPermissionUtils::isAutomotiveDevice() {
    // Checks the property ro.hardware.type and returns true if it is
    // automotive.
    char value[PROPERTY_VALUE_MAX] = {0};
    property_get("ro.hardware.type", value, "");
    return strncmp(value, "automotive", PROPERTY_VALUE_MAX) == 0;
}

bool AttributionAndPermissionUtils::isHeadlessSystemUserMode() {
    // Checks if the device is running in headless system user mode
    // by checking the property ro.fw.mu.headless_system_user.
    char value[PROPERTY_VALUE_MAX] = {0};
    property_get("ro.fw.mu.headless_system_user", value, "");
    return strncmp(value, "true", PROPERTY_VALUE_MAX) == 0;
}

bool AttributionAndPermissionUtils::isAutomotivePrivilegedClient(int32_t uid) {
    // Returns false if this is not an automotive device type.
    if (!isAutomotiveDevice())
        return false;

    // Returns true if the uid is AID_AUTOMOTIVE_EVS which is a
    // privileged client uid used for safety critical use cases such as
    // rear view and surround view.
    return uid == AID_AUTOMOTIVE_EVS;
}

status_t AttributionAndPermissionUtils::getUidForPackage(const std::string &packageName,
        int userId, /*inout*/uid_t& uid, int err) {
    PermissionController pc;
    uid = pc.getPackageUid(toString16(packageName), 0);
    if (uid <= 0) {
        ALOGE("Unknown package: '%s'", packageName.c_str());
        dprintf(err, "Unknown package: '%s'\n", packageName.c_str());
        return BAD_VALUE;
    }

    if (userId < 0) {
        ALOGE("Invalid user: %d", userId);
        dprintf(err, "Invalid user: %d\n", userId);
        return BAD_VALUE;
    }

    uid = multiuser_get_uid(userId, uid);
    return NO_ERROR;
}

bool AttributionAndPermissionUtils::isCallerCameraServerNotDelegating() {
    return (getCallingPid() == getpid());
}

bool AttributionAndPermissionUtils::hasPermissionsForCamera(const std::string& cameraId,
        const AttributionSourceState& attributionSource) {
    return checkPermissionForPreflight(cameraId, sCameraPermission,
            attributionSource, std::string(), AppOpsManager::OP_NONE);
}

bool AttributionAndPermissionUtils::hasPermissionsForSystemCamera(const std::string& cameraId,
        const AttributionSourceState& attributionSource, bool checkCameraPermissions) {
    bool systemCameraPermission = checkPermissionForPreflight(cameraId,
            sSystemCameraPermission, attributionSource, std::string(), AppOpsManager::OP_NONE);
    return systemCameraPermission && (!checkCameraPermissions
            || hasPermissionsForCamera(cameraId, attributionSource));
}

bool AttributionAndPermissionUtils::hasPermissionsForCameraHeadlessSystemUser(
        const std::string& cameraId, const AttributionSourceState& attributionSource) {
    return checkPermissionForPreflight(cameraId, sCameraHeadlessSystemUserPermission,
            attributionSource, std::string(), AppOpsManager::OP_NONE);
}

bool AttributionAndPermissionUtils::hasPermissionsForCameraPrivacyAllowlist(
        const AttributionSourceState& attributionSource) {
    return checkPermissionForPreflight(std::string(), sCameraPrivacyAllowlistPermission,
            attributionSource, std::string(), AppOpsManager::OP_NONE);
}

bool AttributionAndPermissionUtils::hasPermissionsForOpenCloseListener(
        const AttributionSourceState& attributionSource) {
    return checkPermissionForPreflight(std::string(), sCameraOpenCloseListenerPermission,
            attributionSource, std::string(), AppOpsManager::OP_NONE);
}

} // namespace android
