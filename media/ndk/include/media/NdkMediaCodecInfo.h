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

/**
 * @addtogroup Media
 * @{
 */

/**
 * @file NdkMediaCodecInfo.h
 */

/*
 * This file defines an NDK API.
 * Do not remove methods.
 * Do not change method signatures.
 * Do not change the value of constants.
 * Do not change the size of any of the classes defined in here.
 * Do not reference types that are not part of the NDK.
 * Do not #include files that aren't part of the NDK.
 */

#ifndef _NDK_MEDIA_CODEC_INFO_H
#define _NDK_MEDIA_CODEC_INFO_H

#include "NdkMediaError.h"
#include "NdkMediaFormat.h"

__BEGIN_DECLS

struct ACodecAudioCapabilities;
typedef struct ACodecAudioCapabilities ACodecAudioCapabilities;
struct ACodecPerformancePoint;
typedef struct ACodecPerformancePoint ACodecPerformancePoint;
struct ACodecVideoCapabilities;
typedef struct ACodecVideoCapabilities ACodecVideoCapabilities;
struct ACodecEncoderCapabilities;
typedef struct ACodecEncoderCapabilities ACodecEncoderCapabilities;
struct AMediaCodecInfo;
typedef struct AMediaCodecInfo AMediaCodecInfo;

/**
 * A uitlity structure describing the range of two integer values.
 */
typedef struct AIntRange {
    int32_t mLower;
    int32_t mUpper;
} AIntRange;

/**
 * A uitlity structure describing the range of two double values.
 */
typedef struct ADoubleRange {
    double mLower;
    double mUpper;
} ADoubleRange;

// AMediaCodecInfo

/**
 * Get the canonical name of a codec.
 *
 * \return      The char pointer to the canonical name.
 *              It is owned by the framework. No lifetime management needed for users.
 *
 * Return NULL if @param info is invalid.
 */
const char* AMediaCodecInfo_getCanonicalName(const AMediaCodecInfo *info) __INTRODUCED_IN(36);

/**
 * Query if the codec is an encoder.
 */
bool AMediaCodecInfo_isEncoder(const AMediaCodecInfo *info) __INTRODUCED_IN(36);

/**
 * Query if the codec is provided by the Android platform (false) or the device manufacturer (true).
 */
bool AMediaCodecInfo_isVendor(const AMediaCodecInfo *info) __INTRODUCED_IN(36);

/**
 * The type of codecs.
 */
typedef enum AMediaCodecType : int32_t {
    /**
     * Not a codec type. Used for indicating an invalid operation occurred.
     */
    INVALID_CODEC_INFO = 0,

    /**
     * Software codec.
     *
     * Software-only codecs are more secure as they run in a tighter security sandbox.
     * On the other hand, software-only codecs do not provide any performance guarantees.
     */
    SOFTWARE_ONLY = 1,

    /**
     * Hardware accelerated codec.
     *
     * Hardware codecs generally have higher performance or lower power consumption than
     * software codecs, but since they are specific to each device,
     * the actual performance details can vary.
     */
    HARDWARE_ACCELERATED = 2,

    /**
     * Software codec but have device access.
     * Mainly referring to software codecs provided by vendors.
     */
    SOFTWARE_WITH_DEVICE_ACCESS = 3,
} AMediaCodecType;

/**
 * Query if the codec is SOFTWARE_ONLY, HARDWARE_ACCELERATED or SOFTWARE_WITH_DEVICE_ACCESS.
 *
 * Return INVALID_CODEC_INFO if @param info is invalid.
 */
AMediaCodecType AMediaCodecInfo_getMediaCodecInfoType(
        const AMediaCodecInfo *info) __INTRODUCED_IN(36);

/**
 * Get the supported media type of the codec.
 *
 * \return  The char pointer to the media type.
 *          It is owned by the framework with infinite lifetime.
 *
 * Return NULL if @param info is invalid.
 */
const char* AMediaCodecInfo_getMediaType(const AMediaCodecInfo *info) __INTRODUCED_IN(36);

/**
 * Returns the max number of the supported concurrent codec instances.
 *
 * This is a hint for an upper bound. Applications should not expect to successfully
 * operate more instances than the returned value, but the actual number of
 * concurrently operable instances may be less as it depends on the available
 * resources at time of use.
 *
 * Return -1 if @param info is invalid.
 */
int32_t AMediaCodecInfo_getMaxSupportedInstances(const AMediaCodecInfo *info) __INTRODUCED_IN(36);

/**
 * Query codec feature capabilities.
 *
 * These features are supported to be used by the codec.  These
 * include optional features that can be turned on, as well as
 * features that are always on.
 *
 * Return 1 if the feature is supported;
 * Return 0 if the feature is unsupported;
 * Return -1 if @param featureName is invalid.
 */
int32_t AMediaCodecInfo_isFeatureSupported(const AMediaCodecInfo *info,
        const char *featureName) __INTRODUCED_IN(36);

/**
 * Query codec feature requirements.
 *
 * These features are required to be used by the codec, and as such,
 * they are always turned on.
 *
 * Return 1 if the feature is required;
 * Return 0 if the feature is not required;
 * Return -1 if @param featureName is invalid.
 */
int32_t AMediaCodecInfo_isFeatureRequired(const AMediaCodecInfo *info,
        const char *featureName) __INTRODUCED_IN(36);

/**
 * Query whether codec supports a given @param format.
 *
 * Return 1 if the format is supported;
 * Return 0 if the format is unsupported;
 * Return -1 if @param format is invalid.
 */
int32_t AMediaCodecInfo_isFormatSupported(const AMediaCodecInfo *info,
        const AMediaFormat *format) __INTRODUCED_IN(36);

/**
 * Get the ACodecAudioCapabilities from the given AMediaCodecInfo.
 *
 * @param outAudioCaps        The pointer to the output ACodecAudioCapabilities.
 *                            It is owned by the framework and has an infinite lifetime.
 *
 * Return AMEDIA_OK if successfully got the ACodecAudioCapabilities.
 * Return AMEDIA_ERROR_UNSUPPORTED if the codec is not an audio codec.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if @param info is invalid.
 */
media_status_t AMediaCodecInfo_getAudioCapabilities(const AMediaCodecInfo *info,
        const ACodecAudioCapabilities **outAudioCaps) __INTRODUCED_IN(36);

/**
 * Get the ACodecVideoCapabilities from the given AMediaCodecInfo.
 *
 * @param outVideoCaps        The pointer to the output ACodecVideoCapabilities.
 *                            It is owned by the framework and has an infinite lifetime.
 *
 * Return AMEDIA_OK if successfully got the ACodecVideoCapabilities.
 * Return AMEDIA_ERROR_UNSUPPORTED if the codec is not a video codec.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if @param info is invalid.
 */
media_status_t AMediaCodecInfo_getVideoCapabilities(const AMediaCodecInfo *info,
        const ACodecVideoCapabilities **outVideoCaps) __INTRODUCED_IN(36);

/**
 * Get the ACodecEncoderCapabilities from the given AMediaCodecInfo.
 *
 * @param outEncoderCaps        The pointer to the output ACodecEncoderCapabilities.
 *                              It is owned by the framework and has an infinite lifetime.
 *
 * Return AMEDIA_OK if successfully got the ACodecEncoderCapabilities.
 * Return AMEDIA_ERROR_UNSUPPORTED if the codec is not an encoder.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if @param info is invalid.
 */
media_status_t AMediaCodecInfo_getEncoderCapabilities(const AMediaCodecInfo *info,
        const ACodecEncoderCapabilities **outEncoderCaps) __INTRODUCED_IN(36);

// ACodecAudioCapabilities

/**
 * Get the range of supported bitrates in bits/second.
 *
 * @param outRange  The pointer to the range of supported bitrates.
 *                  Users are responsible for allocating a valid AIntRange structure and
 *                  managing the lifetime of it.
 *
 * Return AMEDIA_OK if got bitrates successfully.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param audioCaps and @param outRange is invalid.
 */
media_status_t ACodecAudioCapabilities_getBitrateRange(const ACodecAudioCapabilities *audioCaps,
        AIntRange *outRange) __INTRODUCED_IN(36);

/**
 * Get the array of supported sample rates
 *
 * The array is sorted in ascending order.
 *
 * @param outArrayPtr   The pointer to the output sample rates array.
 *                      The array is owned by the framework and has an infinite lifetime.
 * @param outCount      The size of the output array.
 *
 * Return AMEDIA_OK if the codec supports only discrete values.
 * Otherwise, it returns AMEDIA_ERROR_UNSUPPORTED.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param audioCaps, @param outArrayPtr
 * and @param outCount is invalid.
 */
media_status_t ACodecAudioCapabilities_getSupportedSampleRates(
        const ACodecAudioCapabilities *audioCaps, const int **outArrayPtr,
        size_t *outCount) __INTRODUCED_IN(36);

/**
 * Get the array of supported sample rate ranges.
 *
 * The array is sorted in ascending order, and the ranges are distinct (non-intersecting).
 *
 * @param outArrayPtr   The pointer to the out sample rate ranges array.
 *                      The array is owned by the framework and has an infinite lifetime.
 * @param outCount      The size of the out array.
 *
 * Return AMEDIA_OK if got the sample rate ranges successfully.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param audioCaps, @param outArrayPtr
 * and @param outCount is invalid.
 */
media_status_t ACodecAudioCapabilities_getSupportedSampleRateRanges(
        const ACodecAudioCapabilities *audioCaps,
        const AIntRange **outArrayPtr, size_t *outCount) __INTRODUCED_IN(36);

/**
 * Return the maximum number of input channels supported.
 *
 * Return -1 if @param audioCaps is invalid.
 */
int32_t ACodecAudioCapabilities_getMaxInputChannelCount(
        const ACodecAudioCapabilities *audioCaps) __INTRODUCED_IN(36);

/**
 * Returns the minimum number of input channels supported.
 * This is often 1, but does vary for certain mime types.
 *
 * Return -1 if @param audioCaps is invalid.
 */
int32_t ACodecAudioCapabilities_getMinInputChannelCount(
        const ACodecAudioCapabilities *audioCaps) __INTRODUCED_IN(36);

/**
 * Get an array of ranges representing the number of input channels supported.
 * The codec supports any number of input channels within this range.
 * For many codecs, this will be a single range [1..N], for some N.
 *
 * The array is sorted in ascending order, and the ranges are distinct (non-intersecting).
 *
 * @param outArrayPtr   The pointer to the output array of input-channels ranges.
 *                      The array is owned by the framework and has an infinite lifetime.
 * @param outCount      The size of the output array.
 *
 * Return AMEDIA_OK if got the input channel array successfully.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if @param audioCaps is invalid.
 */
media_status_t ACodecAudioCapabilities_getInputChannelCountRanges(
        const ACodecAudioCapabilities *audioCaps,
        const AIntRange **outArrayPtr, size_t *outCount) __INTRODUCED_IN(36);

/**
 * Query whether the sample rate is supported by the codec.
 *
 * Return 1 if the sample rate is supported.
 * Return 0 if the sample rate is unsupported
 * Return -1 if @param audioCaps is invalid.
 */
int32_t ACodecAudioCapabilities_isSampleRateSupported(const ACodecAudioCapabilities *audioCaps,
        int32_t sampleRate) __INTRODUCED_IN(36);

// ACodecPerformancePoint

/**
 * Create a performance point for a given frame size and frame rate.
 *
 * Performance points are defined by number of pixels, pixel rate and frame rate.
 *
 * Users are responsible for calling
 * ACodecPerformancePoint_delete(ACodecPerformancePoint *performancePoint) after use.
 *
 * @param width width of the frame in pixels
 * @param height height of the frame in pixels
 * @param frameRate frame rate in frames per second
 */
ACodecPerformancePoint* ACodecPerformancePoint_create(int32_t width, int32_t height,
        int32_t frameRate) __INTRODUCED_IN(36);

/**
 * Delete a created performance point.
 *
 * Return AMEDIA_OK if it is successfully deleted.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if @param performancePoint is invalid.
 */
media_status_t ACodecPerformancePoint_delete(
        ACodecPerformancePoint *performancePoint) __INTRODUCED_IN(36);

/**
 * Checks whether the performance point covers a media format.
 *
 * @param format Stream format considered.
 * Return true if the performance point covers the format.
 */
bool ACodecPerformancePoint_coversFormat(const ACodecPerformancePoint *performancePoint,
        const AMediaFormat *format) __INTRODUCED_IN(36);

/**
 * Checks whether a performance point covers another performance point.
 *
 * Use this method to determine if a performance point advertised by a codec covers the
 * performance point required. This method can also be used for loose ordering as this
 * method is transitive.
 *
 * A Performance point represents an upper bound. This means that
 * it covers all performance points with fewer pixels, pixel rate and frame rate.
 *
 * Return true if @param one covers @param another.
 */
bool ACodecPerformancePoint_covers(const ACodecPerformancePoint *one,
        const ACodecPerformancePoint *another) __INTRODUCED_IN(36);

/**
 * Checks whether two performance points are equal.
 */
bool ACodecPerformancePoint_equals(const ACodecPerformancePoint *one,
        const ACodecPerformancePoint *another) __INTRODUCED_IN(36);

// ACodecVideoCapabilities

/**
 * Get the range of supported bitrates in bits/second.
 *
 * @param outRange  The pointer to the range of output bitrates.
 *                  Users are responsible for allocating a valid AIntRange structure and
 *                  managing the lifetime of it.
 *
 * Return AMEDIA_OK if got the supported bitrates successfully.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param videoCaps and @param outRange is invalid.
 */
media_status_t ACodecVideoCapabilities_getBitrateRange(const ACodecVideoCapabilities *videoCaps,
        AIntRange *outRange) __INTRODUCED_IN(36);

/**
 * Get the range of supported video widths.
 *
 * @param outRange  The pointer to the range of output supported widths.
 *                  Users are responsible for allocating a valid AIntRange structure and
 *                  managing the lifetime of it.
 *
 * Return AMEDIA_OK if got the supported video widths successfully.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param videoCaps and @param outRange is invalid.
 */
media_status_t ACodecVideoCapabilities_getSupportedWidths(const ACodecVideoCapabilities *videoCaps,
        AIntRange *outRange) __INTRODUCED_IN(36);

/**
 * Get the range of supported video heights.
 *
 * @param outRange  The pointer to the range of output supported heights.
 *                  Users are responsible for allocating a valid AIntRange structure and
 *                  managing the lifetime of it.
 *
 * Return AMEDIA_OK if got the supported video heights successfully.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param videoCaps and @param outRange is invalid.
 */
media_status_t ACodecVideoCapabilities_getSupportedHeights(const ACodecVideoCapabilities *videoCaps,
        AIntRange *outRange) __INTRODUCED_IN(36);

/**
 * Return the alignment requirement for video width (in pixels).
 *
 * This is a power-of-2 value that video width must be a multiple of.
 *
 * Return -1 if @param videoCaps is invalid.
 */
int32_t ACodecVideoCapabilities_getWidthAlignment(
        const ACodecVideoCapabilities *videoCaps) __INTRODUCED_IN(36);

/**
 * Return the alignment requirement for video height (in pixels).
 *
 * This is a power-of-2 value that video height must be a multiple of.
 *
 * Return -1 if @param videoCaps is invalid.
 */
int32_t ACodecVideoCapabilities_getHeightAlignment(
        const ACodecVideoCapabilities *videoCaps) __INTRODUCED_IN(36);

/**
 * Get the range of supported frame rates.
 *
 * This is not a performance indicator. Rather, it expresses the limits specified in the coding
 * standard, based on the complexities of encoding material for later playback at a certain
 * frame rate, or the decoding of such material in non-realtime.
 *
 * @param outRange  The pointer to the range of output supported frame rates.
 *                  Users are responsible for allocating a valid AIntRange structure and
 *                  managing the lifetime of it.
 *
 * \return AMEDIA_OK if got the frame rate range successfully.
 * \return AMEDIA_ERROR_INVALID_PARAMETER if any of @param videoCaps and @param outRange is invalid.
 */
media_status_t ACodecVideoCapabilities_getSupportedFrameRates(
        const ACodecVideoCapabilities *videoCaps, AIntRange *outRange) __INTRODUCED_IN(36);

/**
 * Get the range of supported video widths for a video height.
 *
 * @param outRange      The pointer to the range of supported widths.
 *                      Users are responsible for allocating a valid AIntRange structure and
 *                      managing the lifetime of it.
 *
 * Return AMEDIA_OK if got the supported video width range successfully.
 * Return AMEDIA_ERROR_UNSUPPORTED if the height query is not supported.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param videoCaps and @param outRange is invalid.
 */
media_status_t ACodecVideoCapabilities_getSupportedWidthsFor(
        const ACodecVideoCapabilities *videoCaps, int32_t height,
        AIntRange *outRange) __INTRODUCED_IN(36);

/**
 * Get the range of supported video heights for a video width.
 *
 * @param outRange      The pointer to the range of supported heights.
 *                      Users are responsible for allocating a valid AIntRange structure and
 *                      managing the lifetime of it.
 *
 * Return AMEDIA_OK if got the supported video height range successfully.
 * Return AMEDIA_ERROR_UNSUPPORTED if the width query is not supported.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param videoCaps and @param outRange is invalid.
 */
media_status_t ACodecVideoCapabilities_getSupportedHeightsFor(
        const ACodecVideoCapabilities *videoCaps, int32_t width,
        AIntRange *outRange) __INTRODUCED_IN(36);

/**
 * Get the range of supported video frame rates for a video size.
 *
 * This is not a performance indicator.  Rather, it expresses the limits specified in the coding
 * standard, based on the complexities of encoding material of a given size for later playback at
 * a certain frame rate, or the decoding of such material in non-realtime.
 *
 * @param outRange      The pointer to the range of frame rates.
 *                      Users are responsible for allocating a valid ADoubleRange structure and
 *                      managing the lifetime of it.
 *
 * Return AMEDIA_OK if got the supported video frame rates successfully.
 * Return AMEDIA_ERROR_UNSUPPORTED if the size query is not supported.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param videoCaps and @param outRange is invalid.
 */
media_status_t ACodecVideoCapabilities_getSupportedFrameRatesFor(
        const ACodecVideoCapabilities *videoCaps, int32_t width, int32_t height,
        ADoubleRange *outRange) __INTRODUCED_IN(36);

/**
 * Get the range of achievable video frame rates for a video size.
 *
 * This is based on manufacturer's performance measurements for this device and codec.
 * The measurements may not be available for all codecs or devices.
 *
 * @param outRange      The pointer to the range of frame rates.
  *                     Users are responsible for allocating a valid ADoubleRange structure and
 *                      managing the lifetime of it.
 *
 * Return AMEDIA_OK if got the achievable video frame rates successfully.
 * Return AMEDIA_ERROR_UNSUPPORTED if the codec did not publish any measurement data.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param videoCaps and @param outRange is invalid.
 */
media_status_t ACodecVideoCapabilities_getAchievableFrameRatesFor(
        const ACodecVideoCapabilities *videoCaps, int32_t width, int32_t height,
        ADoubleRange *outRange) __INTRODUCED_IN(36);

/**
 * Get the supported performance points.
 *
 * @param outPerformancePointArray      The pointer to the output performance points array.
 *                                      The array is owned by the framework and has an infinite
 *                                      lifetime.
 * @param outCount                      The size of the output array.
 *
 * Return AMEDIA_OK if successfully got the performance points.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if @param videoCaps is invalid.
 */
media_status_t ACodecVideoCapabilities_getSupportedPerformancePoints(
        const ACodecVideoCapabilities *videoCaps,
        const ACodecPerformancePoint **outPerformancePointArray,
        size_t *outCount) __INTRODUCED_IN(36);

/**
 * Return whether a given video size and frameRate combination is supported.
 *
 * Return 1 if the size and rate are supported.
 * Return 0 if they are not supported.
 * Return -1 if @param videoCaps is invalid.
 */
int32_t ACodecVideoCapabilities_areSizeAndRateSupported(const ACodecVideoCapabilities *videoCaps,
        int32_t width, int32_t height, double frameRate) __INTRODUCED_IN(36);

/**
 * Return whether a given video size is supported.
 *
 * Return 1 if the size is supported.
 * Return 0 if the size is not supported.
 * Return -1 if @param videoCaps is invalid.
 */
int32_t ACodecVideoCapabilities_isSizeSupported(const ACodecVideoCapabilities *videoCaps,
        int32_t width, int32_t height) __INTRODUCED_IN(36);

// ACodecEncoderCapabilities

/**
 * Get the supported range of quality values.
 *
 * Quality is implementation-specific. As a general rule, a higher quality
 * setting results in a better image quality and a lower compression ratio.
 *
 * @param outRange      The pointer to the range of quality values.
 *                      Users are responsible for allocating a valid AIntRange structure and
 *                      managing the lifetime of it.
 *
 * Return AMEDIA_OK if successfully got the quality range.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param videoCaps and @param outRange is invalid.
 */
media_status_t ACodecEncoderCapabilities_getQualityRange(
        const ACodecEncoderCapabilities *encoderCaps,
        AIntRange *outRange) __INTRODUCED_IN(36);

/**
 * Get the supported range of encoder complexity values.
 *
 * Some codecs may support multiple complexity levels, where higher complexity values use more
 * encoder tools (e.g. perform more intensive calculations) to improve the quality or the
 * compression ratio. Use a lower value to save power and/or time.
 *
 * @param outRange      The pointer to the range of encoder complexity values.
 *                      Users are responsible for allocating a valid AIntRange structure and
 *                      managing the lifetime of it.
 *
 * Return AMEDIA_OK if successfully got the complexity range.
 * Return AMEDIA_ERROR_INVALID_PARAMETER if any of @param videoCaps and @param outRange is invalid.
 */
media_status_t ACodecEncoderCapabilities_getComplexityRange(
        const ACodecEncoderCapabilities *encoderCaps,
        AIntRange *outRange) __INTRODUCED_IN(36);

/**
 * Encoder bitrate modes.
 */
typedef enum ABiterateMode : int32_t {
    BITRATE_MODE_CQ = 0,
    BITRATE_MODE_VBR = 1,
    BITRATE_MODE_CBR = 2,
    BITRATE_MODE_CBR_FD = 3
} ABiterateMode;

/**
 * Query whether a bitrate mode is supported.
 *
 * Return 1 if the bitrate mode is supported.
 * Return 0 if the bitrate mode is unsupported.
 * Return -1 if @param encoderCaps is invalid.
 */
int32_t ACodecEncoderCapabilities_isBitrateModeSupported(
        const ACodecEncoderCapabilities *encoderCaps, ABiterateMode mode) __INTRODUCED_IN(36);

__END_DECLS

#endif //_NDK_MEDIA_CODEC_INFO_H

/** @} */