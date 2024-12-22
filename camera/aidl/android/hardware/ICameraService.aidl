/*
 * Copyright (C) 2013 The Android Open Source Project
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

package android.hardware;

import android.content.AttributionSourceState;
import android.hardware.ICamera;
import android.hardware.ICameraClient;
import android.hardware.camera2.ICameraDeviceUser;
import android.hardware.camera2.ICameraDeviceCallbacks;
import android.hardware.camera2.ICameraInjectionCallback;
import android.hardware.camera2.ICameraInjectionSession;
import android.hardware.camera2.params.SessionConfiguration;
import android.hardware.camera2.params.VendorTagDescriptor;
import android.hardware.camera2.params.VendorTagDescriptorCache;
import android.hardware.camera2.utils.ConcurrentCameraIdCombination;
import android.hardware.camera2.utils.CameraIdAndSessionConfiguration;
import android.hardware.camera2.impl.CameraMetadataNative;
import android.hardware.ICameraServiceListener;
import android.hardware.CameraInfo;
import android.hardware.CameraStatus;
import android.hardware.CameraExtensionSessionStats;

/**
 * Binder interface for the native camera service running in mediaserver.
 *
 * @hide
 */
interface ICameraService
{
    /**
     * All camera service and device Binder calls may return a
     * ServiceSpecificException with the following error codes
     */
    const int ERROR_PERMISSION_DENIED = 1;
    const int ERROR_ALREADY_EXISTS = 2;
    const int ERROR_ILLEGAL_ARGUMENT = 3;
    const int ERROR_DISCONNECTED = 4;
    const int ERROR_TIMED_OUT = 5;
    const int ERROR_DISABLED = 6;
    const int ERROR_CAMERA_IN_USE = 7;
    const int ERROR_MAX_CAMERAS_IN_USE = 8;
    const int ERROR_DEPRECATED_HAL = 9;
    const int ERROR_INVALID_OPERATION = 10;

    /**
     * Types for getNumberOfCameras
     */
    const int CAMERA_TYPE_BACKWARD_COMPATIBLE = 0;
    const int CAMERA_TYPE_ALL = 1;

    /**
     * Return the number of camera devices available in the system.
     *
     * @param type The type of the camera, can be either CAMERA_TYPE_BACKWARD_COMPATIBLE
     *        or CAMERA_TYPE_ALL.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     */
    int getNumberOfCameras(int type, in AttributionSourceState clientAttribution, int devicePolicy);

    /**
     * If changed, reflect in
     * frameworks/base/core/java/android/hardware/camera2/CameraManager.java.
     * We have an enum here since the decision to override to portrait mode / fetch the
     * rotationOverride as it exists in CameraManager right now is based on a static system
     * property and not something that changes based dynamically, say on fold state. As a result,
     * we can't use just a boolean to differentiate between the case where cameraserver should
     * override to portrait (sensor orientation is 0, 180) or just rotate the sensor feed (sensor
     * orientation is 90, 270)
     */
    const int ROTATION_OVERRIDE_NONE = 0;
    const int ROTATION_OVERRIDE_OVERRIDE_TO_PORTRAIT = 1;
    const int ROTATION_OVERRIDE_ROTATION_ONLY = 2;

    /**
     * Fetch basic camera information for a camera.
     *
     * @param cameraId The ID of the camera to fetch information for.
     * @param rotationOverride Whether to override the sensor orientation information to
     *        correspond to portrait: {@link ICameraService#ROTATION_OVERRIDE_OVERRIDE_TO_PORTRAIT}
     *        will override the sensor orientation and rotate and crop, while {@link
     *        ICameraService#ROTATION_OVERRIDE_ROTATION_ONLY} will rotate and crop the camera feed
     *        without changing the sensor orientation.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     * @return CameraInfo for the camera.
     */
    CameraInfo getCameraInfo(int cameraId, int rotationOverride,
            in AttributionSourceState clientAttribution, int devicePolicy);

    /**
     * Default UID/PID values for non-privileged callers of connect() and connectDevice(). Can be
     * used to set the pid/uid fields of AttributionSourceState to indicate the calling uid/pid
     * should be used.
     */
    const int USE_CALLING_UID = -1;
    const int USE_CALLING_PID = -1;

    /**
     * Open a camera device through the old camera API.
     *
     * @param cameraId The ID of the camera to open.
     * @param targetSdkVersion the target sdk level of the application calling this function.
     * @param rotationOverride Whether to override the sensor orientation information to
     *        correspond to portrait: {@link ICameraService#ROTATION_OVERRIDE_OVERRIDE_TO_PORTRAIT}
     *        will override the sensor orientation and rotate and crop, while {@link
     *        ICameraService#ROTATION_OVERRIDE_ROTATION_ONLY} will rotate and crop the camera feed
     *        without changing the sensor orientation.
     * @param forceSlowJpegMode Whether to force slow jpeg mode.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     */
    ICamera connect(ICameraClient client,
            int cameraId,
            int targetSdkVersion,
            int rotationOverride,
            boolean forceSlowJpegMode,
            in AttributionSourceState clientAttribution,
            int devicePolicy);

    /**
     * Open a camera device through the new camera API.
     * Only supported for device HAL versions >= 3.2.
     *
     * @param cameraId The ID of the camera to open.
     * @param targetSdkVersion the target sdk level of the application calling this function.
     * @param rotationOverride Whether to override the sensor orientation information to
     *        correspond to portrait: {@link ICameraService#ROTATION_OVERRIDE_OVERRIDE_TO_PORTRAIT}
     *        will override the sensor orientation and rotate and crop, while {@link
     *        ICameraService#ROTATION_OVERRIDE_ROTATION_ONLY} will rotate and crop the camera feed
     *        without changing the sensor orientation.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     */
    ICameraDeviceUser connectDevice(ICameraDeviceCallbacks callbacks,
            @utf8InCpp String cameraId,
            int oomScoreOffset,
            int targetSdkVersion,
            int rotationOverride,
            in AttributionSourceState clientAttribution,
            int devicePolicy);

    /**
     * Add listener for changes to camera device and flashlight state.
     *
     * Also returns the set of currently-known camera IDs and state of each device.
     * Adding a listener will trigger the torch status listener to fire for all
     * devices that have a flash unit.
     */
    CameraStatus[] addListener(ICameraServiceListener listener);

    /**
     * Get a list of combinations of camera ids which support concurrent streaming.
     *
     */
    ConcurrentCameraIdCombination[] getConcurrentCameraIds();

    /**
     * Check whether a particular set of session configurations are concurrently supported by the
     * corresponding camera ids.
     *
     * @param sessions the set of camera id and session configuration pairs to be queried.
     * @param targetSdkVersion the target sdk level of the application calling this function.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     * @return true  - the set of concurrent camera id and stream combinations is supported.
     *         false - the set of concurrent camera id and stream combinations is not supported
     *                 OR the method was called with a set of camera ids not returned by
     *                 getConcurrentCameraIds().
     */
    boolean isConcurrentSessionConfigurationSupported(
            in CameraIdAndSessionConfiguration[] sessions,
            int targetSdkVersion, in AttributionSourceState clientAttribution, int devicePolicy);

    /**
     * Inject Session Params into an existing camera session.
     *
     * @param cameraId the camera id session to inject session params into. Note that
     *                 if there is no active session for the input cameraid, this operation
     *                 will be a no-op. In addition, future camera sessions for cameraid will
     *                 not be affected.
     * @param sessionParams the session params to override for the existing session.
     */
    void injectSessionParams(@utf8InCpp String cameraId,
            in CameraMetadataNative sessionParams);

    /**
     * Remove listener for changes to camera device and flashlight state.
     */
    void removeListener(ICameraServiceListener listener);

    /**
     * Read the static camera metadata for a camera device.
     * Only supported for device HAL versions >= 3.2
     *
     * @param cameraId The ID of the camera to fetch metadata for.
     * @param targetSdkVersion the target sdk level of the application calling this function.
     * @param rotationOverride Whether to override the sensor orientation information to
     *        correspond to portrait: {@link ICameraService#ROTATION_OVERRIDE_OVERRIDE_TO_PORTRAIT}
     *        will override the sensor orientation and rotate and crop, while {@link
     *        ICameraService#ROTATION_OVERRIDE_ROTATION_ONLY} will rotate and crop the camera feed
     *        without changing the sensor orientation.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     * @return Characteristics for the given camera.
     */
    CameraMetadataNative getCameraCharacteristics(@utf8InCpp String cameraId, int targetSdkVersion,
            int rotationOverride, in AttributionSourceState clientAttribution, int devicePolicy);

    /**
     * Read in the vendor tag descriptors from the camera module HAL.
     * Intended to be used by the native code of CameraMetadataNative to correctly
     * interpret camera metadata with vendor tags.
     */
    VendorTagDescriptor getCameraVendorTagDescriptor();

    /**
     * Retrieve the vendor tag descriptor cache which can have multiple vendor
     * providers.
     * Intended to be used by the native code of CameraMetadataNative to correctly
     * interpret camera metadata with vendor tags.
     */
    VendorTagDescriptorCache getCameraVendorTagCache();

    /**
     * Read the legacy camera1 parameters into a String
     */
    @utf8InCpp String getLegacyParameters(int cameraId);

    /**
     * apiVersion constants for supportsCameraApi
     */
    const int API_VERSION_1 = 1;
    const int API_VERSION_2 = 2;

    // Determines if a particular API version is supported directly for a cameraId.
    boolean supportsCameraApi(@utf8InCpp String cameraId, int apiVersion);
    // Determines if a cameraId is a hidden physical camera of a logical multi-camera.
    boolean isHiddenPhysicalCamera(@utf8InCpp String cameraId);
    // Inject the external camera to replace the internal camera session.
    ICameraInjectionSession injectCamera(@utf8InCpp String packageName, @utf8InCpp String internalCamId,
            @utf8InCpp String externalCamId, in ICameraInjectionCallback CameraInjectionCallback);

    /**
     * Set the torch mode for a camera device.
     *
     * @param cameraId The ID of the camera to set torch mode for.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     */
    void setTorchMode(@utf8InCpp String cameraId, boolean enabled, IBinder clientBinder,
            in AttributionSourceState clientAttribution, int devicePolicy);

    /**
     * Change the brightness level of the flash unit associated with cameraId to strengthLevel.
     * If the torch is in OFF state and strengthLevel > 0 then the torch will also be turned ON.
     *
     * @param cameraId The ID of the camera.
     * @param strengthLevel The torch strength level to set for the camera.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     */
    void turnOnTorchWithStrengthLevel(@utf8InCpp String cameraId, int strengthLevel,
            IBinder clientBinder, in AttributionSourceState clientAttribution, int devicePolicy);

    /**
     * Get the brightness level of the flash unit associated with cameraId.
     *
     * @param cameraId The ID of the camera.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     * @return Torch strength level for the camera.
     */
    int getTorchStrengthLevel(@utf8InCpp String cameraId,
            in AttributionSourceState clientAttribution, int devicePolicy);

    /**
     * Notify the camera service of a system event.  Should only be called from system_server.
     *
     * Callers require the android.permission.CAMERA_SEND_SYSTEM_EVENTS permission.
     */
    const int EVENT_NONE = 0;
    const int EVENT_USER_SWITCHED = 1; // The argument is the set of new foreground user IDs.
    const int EVENT_USB_DEVICE_ATTACHED = 2; // The argument is the deviceId and vendorId
    const int EVENT_USB_DEVICE_DETACHED = 3; // The argument is the deviceId and vendorId
    oneway void notifySystemEvent(int eventId, in int[] args);

    /**
     * Notify the camera service of a display configuration change.
     *
     * Callers require the android.permission.CAMERA_SEND_SYSTEM_EVENTS permission.
     */
    oneway void notifyDisplayConfigurationChange();

    /**
     * Notify the camera service of a device physical status change. May only be called from
     * a privileged process.
     *
     * newState is a bitfield consisting of DEVICE_STATE_* values combined together. Valid state
     * combinations are device-specific. At device startup, the camera service will assume the device
     * state is NORMAL until otherwise notified.
     *
     * Callers require the android.permission.CAMERA_SEND_SYSTEM_EVENTS permission.
     */
    oneway void notifyDeviceStateChange(long newState);

    /**
     * Report Extension specific metrics to camera service for logging. This should only be called
     * by CameraExtensionSession to log extension metrics. All calls after the first must set
     * CameraExtensionSessionStats.key to the value returned by this function.
     *
     * Each subsequent call fully overwrites the existing CameraExtensionSessionStats for the
     * current session, so the caller is responsible for keeping the stats complete.
     *
     * Due to cameraservice and cameraservice_proxy architecture, there is no guarantee that
     * {@code stats} will be logged immediately (or at all). CameraService will log whatever
     * extension stats it has at the time of camera session closing which may be before the app
     * process receives a session/device closed callback; so CameraExtensionSession
     * should send metrics to the cameraservice preriodically, and cameraservice must handle calls
     * to this function from sessions that have not been logged yet and from sessions that have
     * already been closed.
     *
     * @return the key that must be used to report updates to previously reported stats.
     */
    @utf8InCpp String reportExtensionSessionStats(in CameraExtensionSessionStats stats);

    // Bitfield constants for notifyDeviceStateChange
    // All bits >= 32 are for custom vendor states
    // Written as ints since AIDL does not support long constants.
    const int DEVICE_STATE_NORMAL = 0;
    const int DEVICE_STATE_BACK_COVERED = 1;
    const int DEVICE_STATE_FRONT_COVERED = 2;
    const int DEVICE_STATE_FOLDED = 4;
    const int DEVICE_STATE_LAST_FRAMEWORK_BIT = 0x80000000; // 1 << 31;

    /**
     * Create a CaptureRequest metadata based on template id
     *
     * @param cameraId The camera id to create the CaptureRequest for.
     * @param templateId The template id create the CaptureRequest for.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     * @return Metadata representing the CaptureRequest.
     */
    CameraMetadataNative createDefaultRequest(@utf8InCpp String cameraId, int templateId,
            in AttributionSourceState clientAttribution, int devicePolicy);

    /**
     * Check whether a particular session configuration with optional session parameters
     * has camera device support.
     *
     * @param cameraId The camera id to query session configuration for
     * @param targetSdkVersion the target sdk level of the application calling this function.
     * @param sessionConfiguration Specific session configuration to be verified.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     * @return true  - in case the stream combination is supported.
     *         false - in case there is no device support.
     */
    boolean isSessionConfigurationWithParametersSupported(@utf8InCpp String cameraId,
            int targetSdkVersion, in SessionConfiguration sessionConfiguration,
            in AttributionSourceState clientAttribution, int devicePolicy);

    /**
     * Get the camera characteristics for a particular session configuration for
     * the given camera device.
     *
     * @param cameraId ID of the device for which the session characteristics must be fetched.
     * @param targetSdkVersion the target sdk level of the application calling this function.
     * @param rotationOverride Whether to override the sensor orientation information to
     *        correspond to portrait: {@link ICameraService#ROTATION_OVERRIDE_OVERRIDE_TO_PORTRAIT}
     *        will override the sensor orientation and rotate and crop, while {@link
     *        ICameraService#ROTATION_OVERRIDE_ROTATION_ONLY} will rotate and crop the camera feed
     *        without changing the sensor orientation.
     * @param sessionConfiguration Session configuration for which the characteristics
     *                             must be fetched.
     * @param clientAttribution The AttributionSource of the client.
     * @param devicePolicy The camera policy of the device of the associated context (default
     *                     policy for default device context). Only virtual cameras would be exposed
     *                     only for custom policy and only real cameras would be exposed for default
     *                     policy.
     * @return Characteristics associated with the given session.
     */
    CameraMetadataNative getSessionCharacteristics(@utf8InCpp String cameraId,
            int targetSdkVersion,
            int rotationOverride,
            in SessionConfiguration sessionConfiguration,
            in AttributionSourceState clientAttribution,
            int devicePolicy);
}
