/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef _ACAMERA_MANAGER_H
#define _ACAMERA_MANAGER_H

#include <camera/NdkCameraManager.h>

#include <android-base/parseint.h>
#include <android/companion/virtualnative/IVirtualDeviceManagerNative.h>
#include <android/hardware/ICameraService.h>
#include <android/hardware/BnCameraServiceListener.h>
#include <camera/CameraMetadata.h>
#include <binder/IServiceManager.h>
#include <utils/StrongPointer.h>
#include <utils/Mutex.h>

#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/AMessage.h>

#include <set>
#include <map>

namespace android {
namespace acam {

enum class DevicePolicy {
  DEVICE_POLICY_DEFAULT =
    ::android::companion::virtualnative::IVirtualDeviceManagerNative::DEVICE_POLICY_DEFAULT,
  DEVICE_POLICY_CUSTOM =
    ::android::companion::virtualnative::IVirtualDeviceManagerNative::DEVICE_POLICY_CUSTOM
};

/**
 * Device context within which are cameras accessed.
 *
 * When constructed, the device id is set to id of virtual device corresponding to
 * caller's UID (or default device id if current app process is not running on virtual device).
 *
 * See getDeviceId() in Context.java for more context (no pun intented).
 */
struct DeviceContext {
    DeviceContext();

    // Id of virtual device asociated with this context (or DEFAULT_DEVICE_ID = 0 in case
    // caller UID is not running on virtual device).
    int deviceId;
    // Device policy corresponding to VirtualDeviceParams.POLICY_TYPE_CAMERA:
    //
    // Can be either:
    // * (0) DEVICE_POLICY_DEFAULT - virtual devices have access to default device cameras.
    // * (1) DEVICE_POLICY_CUSTOM - virtual devices do not have access to default device cameras
    //                              and can only access virtual cameras owned by the same device.
    DevicePolicy policy;
};

/**
 * Per-process singleton instance of CameraManger. Shared by all ACameraManager
 * instances. Created when first ACameraManager is created and destroyed when
 * all ACameraManager instances are deleted.
 *
 * TODO: maybe CameraManagerGlobal is better suited in libcameraclient?
 */
class CameraManagerGlobal final : public RefBase {
  public:
    static sp<CameraManagerGlobal> getInstance();
    sp<hardware::ICameraService> getCameraService();

    void registerAvailabilityCallback(const DeviceContext& context,
                                      const ACameraManager_AvailabilityCallbacks* callback);
    void unregisterAvailabilityCallback(const DeviceContext& context,
                                        const ACameraManager_AvailabilityCallbacks* callback);

    void registerExtendedAvailabilityCallback(
            const DeviceContext& context,
            const ACameraManager_ExtendedAvailabilityCallbacks* callback);
    void unregisterExtendedAvailabilityCallback(
            const DeviceContext& context,
            const ACameraManager_ExtendedAvailabilityCallbacks* callback);

    /**
     * Return camera IDs that support camera2
     */
    void getCameraIdList(const DeviceContext& deviceContext, std::vector<std::string>* cameraIds);

  private:
    sp<hardware::ICameraService> mCameraService;
    const char*                  kCameraServiceName      = "media.camera";
    Mutex                        mLock;

    template <class T>
    void registerAvailCallback(const DeviceContext& deviceContext, const T* callback);

    class DeathNotifier : public IBinder::DeathRecipient {
      public:
        explicit DeathNotifier(CameraManagerGlobal* cm) : mCameraManager(cm) {}
      protected:
        // IBinder::DeathRecipient implementation
        virtual void binderDied(const wp<IBinder>& who);
      private:
        const wp<CameraManagerGlobal> mCameraManager;
    };
    sp<DeathNotifier> mDeathNotifier;

    class CameraServiceListener final : public hardware::BnCameraServiceListener {
      public:
        explicit CameraServiceListener(CameraManagerGlobal* cm) : mCameraManager(cm) {}
        virtual binder::Status onStatusChanged(int32_t status, const std::string& cameraId,
                int32_t deviceId);
        virtual binder::Status onPhysicalCameraStatusChanged(int32_t status,
                const std::string& cameraId, const std::string& physicalCameraId, int32_t deviceId);

        // Torch API not implemented yet
        virtual binder::Status onTorchStatusChanged(int32_t, const std::string&, int32_t) {
            return binder::Status::ok();
        }
        virtual binder::Status onTorchStrengthLevelChanged(const std::string&, int32_t, int32_t) {
            return binder::Status::ok();
        }

        virtual binder::Status onCameraAccessPrioritiesChanged();
        virtual binder::Status onCameraOpened(const std::string&, const std::string&, int32_t) {
            return binder::Status::ok();
        }
        virtual binder::Status onCameraClosed(const std::string&, int32_t) {
            return binder::Status::ok();
        }

      private:
        const wp<CameraManagerGlobal> mCameraManager;
    };
    sp<CameraServiceListener> mCameraServiceListener;

    // Wrapper of ACameraManager_AvailabilityCallbacks so we can store it in std::set
    struct Callback {
        explicit Callback(const DeviceContext& deviceContext,
                 const ACameraManager_AvailabilityCallbacks* callback)
            : mDeviceContext(deviceContext),
              mAvailable(callback->onCameraAvailable),
              mUnavailable(callback->onCameraUnavailable),
              mAccessPriorityChanged(nullptr),
              mPhysicalCamAvailable(nullptr),
              mPhysicalCamUnavailable(nullptr),
              mContext(callback->context) {}

        explicit Callback(const DeviceContext& deviceContext,
                 const ACameraManager_ExtendedAvailabilityCallbacks* callback)
            : mDeviceContext(deviceContext),
              mAvailable(callback->availabilityCallbacks.onCameraAvailable),
              mUnavailable(callback->availabilityCallbacks.onCameraUnavailable),
              mAccessPriorityChanged(callback->onCameraAccessPrioritiesChanged),
              mPhysicalCamAvailable(callback->onPhysicalCameraAvailable),
              mPhysicalCamUnavailable(callback->onPhysicalCameraUnavailable),
              mContext(callback->availabilityCallbacks.context) {}

        bool operator == (const Callback& other) const {
            return (mAvailable == other.mAvailable && mUnavailable == other.mUnavailable &&
                    mAccessPriorityChanged == other.mAccessPriorityChanged &&
                    mPhysicalCamAvailable == other.mPhysicalCamAvailable &&
                    mPhysicalCamUnavailable == other.mPhysicalCamUnavailable &&
                    mContext == other.mContext &&
                    mDeviceContext.deviceId == other.mDeviceContext.deviceId &&
                    mDeviceContext.policy == other.mDeviceContext.policy);
        }
        bool operator != (const Callback& other) const {
            return !(*this == other);
        }
        bool operator < (const Callback& other) const {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wordered-compare-function-pointers"
            if (*this == other) return false;
            if (mDeviceContext.deviceId != other.mDeviceContext.deviceId) {
                return mDeviceContext.deviceId < other.mDeviceContext.deviceId;
            }
            if (mContext != other.mContext) return mContext < other.mContext;
            if (mPhysicalCamAvailable != other.mPhysicalCamAvailable) {
                return mPhysicalCamAvailable < other.mPhysicalCamAvailable;
            }
            if (mPhysicalCamUnavailable != other.mPhysicalCamUnavailable) {
                return mPhysicalCamUnavailable < other.mPhysicalCamUnavailable;
            }
            if (mAccessPriorityChanged != other.mAccessPriorityChanged) {
                return mAccessPriorityChanged < other.mAccessPriorityChanged;
            }
            if (mAvailable != other.mAvailable) return mAvailable < other.mAvailable;
            return mUnavailable < other.mUnavailable;
#pragma GCC diagnostic pop
        }
        bool operator > (const Callback& other) const {
            return (*this != other && !(*this < other));
        }
        DeviceContext mDeviceContext;
        ACameraManager_AvailabilityCallback mAvailable;
        ACameraManager_AvailabilityCallback mUnavailable;
        ACameraManager_AccessPrioritiesChangedCallback mAccessPriorityChanged;
        ACameraManager_PhysicalCameraAvailabilityCallback mPhysicalCamAvailable;
        ACameraManager_PhysicalCameraAvailabilityCallback mPhysicalCamUnavailable;
        void*                               mContext;
    };

    android::Condition mCallbacksCond;
    size_t mPendingCallbackCnt = 0;
    void onCallbackCalled();
    void drainPendingCallbacksLocked();

    std::set<Callback> mCallbacks;

    // definition of handler and message
    enum {
        kWhatSendSingleCallback,
        kWhatSendSingleAccessCallback,
        kWhatSendSinglePhysicalCameraCallback,
    };
    static const char* kCameraIdKey;
    static const char* kPhysicalCameraIdKey;
    static const char* kCallbackFpKey;
    static const char* kContextKey;
    static const nsecs_t kCallbackDrainTimeout;
    class CallbackHandler : public AHandler {
      public:
        CallbackHandler(wp<CameraManagerGlobal> parent) : mParent(parent) {}
        void onMessageReceived(const sp<AMessage> &msg) override;

      private:
        wp<CameraManagerGlobal> mParent;
        void notifyParent();
        void onMessageReceivedInternal(const sp<AMessage> &msg);
    };
    sp<CallbackHandler> mHandler;
    sp<ALooper>         mCbLooper; // Looper thread where callbacks actually happen on

    sp<hardware::ICameraService> getCameraServiceLocked();
    void onCameraAccessPrioritiesChanged();
    void onStatusChanged(int32_t status, int deviceId, const std::string& cameraId);
    void onStatusChangedLocked(int32_t status, int deviceId, const std::string& cameraId);
    void onStatusChanged(int32_t status, int deviceId, const std::string& cameraId,
                         const std::string& physicalCameraId);
    void onStatusChangedLocked(int32_t status, int deviceId, const std::string& cameraId,
                               const std::string& physicalCameraId);
    // Utils for status
    static bool validStatus(int32_t status);
    static bool isStatusAvailable(int32_t status);
    bool supportsCamera2ApiLocked(const std::string &cameraId);

    struct StatusAndHAL3Support {
      private:
        int32_t status = hardware::ICameraServiceListener::STATUS_NOT_PRESENT;
        mutable std::mutex mLock;
        std::set<std::string> unavailablePhysicalIds;
      public:
        const bool supportsHAL3 = false;
        StatusAndHAL3Support(int32_t st, bool HAL3support):
                status(st), supportsHAL3(HAL3support) { };
        StatusAndHAL3Support() = default;

        bool addUnavailablePhysicalId(const std::string& physicalCameraId);
        bool removeUnavailablePhysicalId(const std::string& physicalCameraId);
        int32_t getStatus();
        void updateStatus(int32_t newStatus);
        std::set<std::string> getUnavailablePhysicalIds();
    };

    struct DeviceStatusMapKey {
        int deviceId;
        std::string cameraId;

        bool operator<(const DeviceStatusMapKey& other) const {
            if (deviceId != other.deviceId) {
                return deviceId < other.deviceId;
            }

            // The sort logic must match the logic in
            // libcameraservice/common/CameraProviderManager.cpp::getAPI1CompatibleCameraDeviceIds
            uint32_t cameraIdUint = 0, otherCameraIdUint = 0;
            bool cameraIdIsUint = base::ParseUint(cameraId.c_str(), &cameraIdUint);
            bool otherCameraIdIsUint = base::ParseUint(other.cameraId.c_str(), &otherCameraIdUint);

            // Uint device IDs first
            if (cameraIdIsUint && otherCameraIdIsUint) {
                return cameraIdUint < otherCameraIdUint;
            } else if (cameraIdIsUint) {
                return true;
            } else if (otherCameraIdIsUint) {
                return false;
            }
            // Simple string compare if both id are not uint
            return cameraIdIsUint < otherCameraIdIsUint;
        }
    };

    std::map<DeviceStatusMapKey, StatusAndHAL3Support> mDeviceStatusMap;

    // For the singleton instance
    static Mutex sLock;
    static wp<CameraManagerGlobal> sInstance;
    CameraManagerGlobal() {}
    ~CameraManagerGlobal();
};

} // namespace acam;
} // namespace android;

/**
 * ACameraManager opaque struct definition
 * Leave outside of android namespace because it's NDK struct
 */
struct ACameraManager {
    ACameraManager() : mGlobalManager(android::acam::CameraManagerGlobal::getInstance()) {}
    camera_status_t getCameraIdList(ACameraIdList** cameraIdList);
    static void     deleteCameraIdList(ACameraIdList* cameraIdList);

    camera_status_t getCameraCharacteristics(
            const char* cameraId, android::sp<ACameraMetadata>* characteristics);
    camera_status_t openCamera(const char* cameraId,
                               ACameraDevice_StateCallbacks* callback,
                               /*out*/ACameraDevice** device);
    void registerAvailabilityCallback(const ACameraManager_AvailabilityCallbacks* callback);
    void unregisterAvailabilityCallback(const ACameraManager_AvailabilityCallbacks* callback);
    void registerExtendedAvailabilityCallback(
            const ACameraManager_ExtendedAvailabilityCallbacks* callback);
    void unregisterExtendedAvailabilityCallback(
            const ACameraManager_ExtendedAvailabilityCallbacks* callback);

  private:
    enum {
        kCameraIdListNotInit = -1
    };
    android::Mutex         mLock;
    android::sp<android::acam::CameraManagerGlobal> mGlobalManager;
    const android::acam::DeviceContext mDeviceContext;
};

#endif //_ACAMERA_MANAGER_H
