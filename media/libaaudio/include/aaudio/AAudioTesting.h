/*
 * Copyright (C) 2017 The Android Open Source Project
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

/**
 * This is test support for AAudio.
 */
#ifndef AAUDIO_AAUDIO_TESTING_H
#define AAUDIO_AAUDIO_TESTING_H

#include <aaudio/AAudio.h>

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************************
 * The definitions below are only for testing. Do not use them in an application.
 * They may change or be removed at any time.
 ************************************************************************************/

/**
 * When the audio is played/recorded via AAudio MMAP data path, the apps can write to/read from
 * a shared memory that will also be accessed directly by hardware. That reduces the audio latency.
 * The following values are used to describe how AAudio MMAP is supported.
 */
enum {
    /**
     * AAudio MMAP is disabled and never used.
     */
    AAUDIO_POLICY_NEVER = 1,

    /**
     * AAudio MMAP support depends on device's availability. It will be used
     * when it is possible or fallback to the normal path, where the audio data
     * will be delivered via audio framework data pipeline.
     */
    AAUDIO_POLICY_AUTO,

    /**
     * AAudio MMAP must be used or fail.
     */
    AAUDIO_POLICY_ALWAYS
};
typedef int32_t aaudio_policy_t;

// The values are copied from JAVA SDK device types defined in android/media/AudioDeviceInfo.java
// When a new value is added, it should be added here and handled by the conversion at
// AAudioConvert_aaudioToAndroidDeviceType.
typedef enum AAudio_DeviceType : int32_t {
    /**
     * A device type describing the attached earphone speaker.
     */
    AAUDIO_DEVICE_BUILTIN_EARPIECE = 1,

    /**
     * A device type describing the speaker system (i.e. a mono speaker or stereo speakers) built
     * in a device.
     */
    AAUDIO_DEVICE_BUILTIN_SPEAKER = 2,

    /**
     * A device type describing a headset, which is the combination of a headphones and microphone.
     */
    AAUDIO_DEVICE_WIRED_HEADSET = 3,

    /**
     * A device type describing a pair of wired headphones.
     */
    AAUDIO_DEVICE_WIRED_HEADPHONES = 4,

    /**
     * A device type describing an analog line-level connection.
     */
    AAUDIO_DEVICE_LINE_ANALOG = 5,

    /**
     * A device type describing a digital line connection (e.g. SPDIF).
     */
    AAUDIO_DEVICE_LINE_DIGITAL = 6,

    /**
     * A device type describing a Bluetooth device typically used for telephony.
     */
    AAUDIO_DEVICE_BLUETOOTH_SCO = 7,

    /**
     * A device type describing a Bluetooth device supporting the A2DP profile.
     */
    AAUDIO_DEVICE_BLUETOOTH_A2DP = 8,

    /**
     * A device type describing an HDMI connection .
     */
    AAUDIO_DEVICE_HDMI = 9,

    /**
     * A device type describing the Audio Return Channel of an HDMI connection.
     */
    AAUDIO_DEVICE_HDMI_ARC = 10,

    /**
     * A device type describing a USB audio device.
     */
    AAUDIO_DEVICE_USB_DEVICE = 11,

    /**
     * A device type describing a USB audio device in accessory mode.
     */
    AAUDIO_DEVICE_USB_ACCESSORY = 12,

    /**
     * A device type describing the audio device associated with a dock.
     * Starting at API 34, this device type only represents digital docks, while docks with an
     * analog connection are represented with {@link #AAUDIO_DEVICE_DOCK_ANALOG}.
     */
    AAUDIO_DEVICE_DOCK = 13,

    /**
     * A device type associated with the transmission of audio signals over FM.
     */
    AAUDIO_DEVICE_FM = 14,

    /**
     * A device type describing the microphone(s) built in a device.
     */
    AAUDIO_DEVICE_BUILTIN_MIC = 15,

    /**
     * A device type for accessing the audio content transmitted over FM.
     */
    AAUDIO_DEVICE_FM_TUNER = 16,

    /**
     * A device type for accessing the audio content transmitted over the TV tuner system.
     */
    AAUDIO_DEVICE_TV_TUNER = 17,

    /**
     * A device type describing the transmission of audio signals over the telephony network.
     */
    AAUDIO_DEVICE_TELEPHONY = 18,

    /**
     * A device type describing the auxiliary line-level connectors.
     */
    AAUDIO_DEVICE_AUX_LINE = 19,

    /**
     * A device type connected over IP.
     */
    AAUDIO_DEVICE_IP = 20,

    /**
     * A type-agnostic device used for communication with external audio systems.
     */
    AAUDIO_DEVICE_BUS = 21,

    /**
     * A device type describing a USB audio headset.
     */
    AAUDIO_DEVICE_USB_HEADSET = 22,

    /**
     * A device type describing a Hearing Aid.
     */
    AAUDIO_DEVICE_HEARING_AID = 23,

    /**
     * A device type describing the speaker system (i.e. a mono speaker or stereo speakers) built
     * in a device, that is specifically tuned for outputting sounds like notifications and alarms
     * (i.e. sounds the user couldn't necessarily anticipate).
     * <p>Note that this physical audio device may be the same as {@link #TYPE_BUILTIN_SPEAKER}
     * but is driven differently to safely accommodate the different use case.</p>
     */
    AAUDIO_DEVICE_BUILTIN_SPEAKER_SAFE = 24,

    /**
     * A device type for rerouting audio within the Android framework between mixes and
     * system applications.
     */
    AAUDIO_DEVICE_REMOTE_SUBMIX = 25,
    /**
     * A device type describing a Bluetooth Low Energy (BLE) audio headset or headphones.
     * Headphones are grouped with headsets when the device is a sink:
     * the features of headsets and headphones with regard to playback are the same.
     */
    AAUDIO_DEVICE_BLE_HEADSET = 26,

    /**
     * A device type describing a Bluetooth Low Energy (BLE) audio speaker.
     */
    AAUDIO_DEVICE_BLE_SPEAKER = 27,

    /**
     * A device type describing an Echo Canceller loopback Reference.
     * This device is only used when capturing with MediaRecorder.AudioSource.ECHO_REFERENCE,
     * which requires privileged permission
     * {@link android.Manifest.permission#CAPTURE_AUDIO_OUTPUT}.
     *
     * Note that this is not exposed as it is a system API that requires privileged permission.
     */
    // AAUDIO_DEVICE_ECHO_REFERENCE = 28,

    /**
     * A device type describing the Enhanced Audio Return Channel of an HDMI connection.
     */
    AAUDIO_DEVICE_HDMI_EARC = 29,

    /**
     * A device type describing a Bluetooth Low Energy (BLE) broadcast group.
     */
    AAUDIO_DEVICE_BLE_BROADCAST = 30,

    /**
     * A device type describing the audio device associated with a dock using an analog connection.
     */
    AAUDIO_DEVICE_DOCK_ANALOG = 31
} AAudio_DeviceType;

/**
 * Query how aaudio mmap is supported for the given device type.
 *
 * @param device device type
 * @param direction {@link AAUDIO_DIRECTION_OUTPUT} or {@link AAUDIO_DIRECTION_INPUT}
 * @return the mmap policy or negative error
 */
AAUDIO_API aaudio_policy_t AAudio_getPlatformMMapPolicy(
        AAudio_DeviceType device, aaudio_direction_t direction) __INTRODUCED_IN(36);

/**
 * Query how aaudio exclusive mmap is supported for the given device type.
 *
 * @param device device type
 * @param direction {@link AAUDIO_DIRECTION_OUTPUT} or {@link AAUDIO_DIRECTION_INPUT}
 * @return the mmap exclusive policy or negative error
 */
AAUDIO_API aaudio_policy_t AAudio_getPlatformMMapExclusivePolicy(
        AAudio_DeviceType device, aaudio_direction_t direction) __INTRODUCED_IN(36);

/**
 * Control whether AAudioStreamBuilder_openStream() will use the new MMAP data path
 * or the older "Legacy" data path.
 *
 * This will only affect the current process.
 *
 * If unspecified then the policy will be based on system properties or configuration.
 *
 * @note This is only for testing. Do not use this in an application.
 * It may change or be removed at any time.
 *
 * @param policy AAUDIO_UNSPECIFIED, AAUDIO_POLICY_NEVER, AAUDIO_POLICY_AUTO, or AAUDIO_POLICY_ALWAYS
 * @return AAUDIO_OK or a negative error
 */
AAUDIO_API aaudio_result_t AAudio_setMMapPolicy(aaudio_policy_t policy);

/**
 * Get the current MMAP policy set by AAudio_setMMapPolicy().
 *
 * @note This is only for testing. Do not use this in an application.
 * It may change or be removed at any time.
 *
 * @return current policy
 */
AAUDIO_API aaudio_policy_t AAudio_getMMapPolicy();

/**
 * Return true if the stream uses the MMAP data path versus the legacy path.
 *
 * @note This is only for testing. Do not use this in an application.
 * It may change or be removed at any time.
 *
 * @return true if the stream uses the MMAP data path
 */
AAUDIO_API bool AAudioStream_isMMapUsed(AAudioStream* stream);

#ifdef __cplusplus
}
#endif

#endif //AAUDIO_AAUDIO_TESTING_H

/** @} */
