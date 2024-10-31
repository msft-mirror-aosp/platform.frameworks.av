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
 * This is the system APIs for AAudio.
 */
#ifndef SYSTEM_AAUDIO_H
#define SYSTEM_AAUDIO_H

#include <aaudio/AAudio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The tags string attributes allows OEMs to extend the
 * <a href="/reference/android/media/AudioAttributes">AudioAttributes</a>.
 *
 * Note that the maximum length includes all tags combined with delimiters and null terminator.
 *
 * Note that it matches the equivalent value in
 * <a href="/reference/android/system/media/audio">AUDIO_ATTRIBUTES_TAGS_MAX_SIZE</a>
 * in the Android native API.
 */
#define AAUDIO_ATTRIBUTES_TAGS_MAX_SIZE 256

/**
 * Set one or more vendor extension tags that the output stream will carry.
 *
 * The tags can be used by the audio policy engine for routing purpose.
 * Routing is based on audio attributes, translated into legacy stream type.
 * The stream types cannot be extended, so the product strategies have been introduced to allow
 * vendor extension of routing capabilities.
 * This could, for example, affect how volume and routing is handled for the stream.
 *
 * The tags can also be used by a System App to pass vendor specific information through the
 * framework to the HAL. That info could affect routing, ducking or other audio behavior in the HAL.
 *
 * By default, audio attributes tags are empty if this method is not called.
 *
 * @param builder reference provided by AAudio_createStreamBuilder()
 * @param tags the desired tags to add, which must be UTF-8 format and null-terminated. The size
 *             of the tags must be at most {@link #AAUDIO_ATTRIBUTES_TAGS_MAX_SIZE}. Multiple tags
 *             must be separated by semicolons.
 * @return {@link #AAUDIO_OK} on success or {@link #AAUDIO_ERROR_ILLEGAL_ARGUMENT} if the given
 *         tags is null or its length is greater than {@link #AAUDIO_ATTRIBUTES_TAGS_MAX_SIZE}.
 */
aaudio_result_t AAudioStreamBuilder_setTags(AAudioStreamBuilder* _Nonnull builder,
                                            const char* _Nonnull tags);

/**
 * Read the audio attributes' tags for the stream into a buffer.
 * The caller is responsible for allocating and freeing the returned data.
 *
 * @param stream reference provided by AAudioStreamBuilder_openStream()
 * @param tags pointer to write the value to in UTF-8 that containing OEM extension tags. It must
 *             be sized with {@link #AAUDIO_ATTRIBUTES_TAGS_MAX_SIZE}.
 * @return {@link #AAUDIO_OK} or {@link #AAUDIO_ERROR_ILLEGAL_ARGUMENT} if the given tags is null.
 */
aaudio_result_t AAudioStream_getTags(AAudioStream* _Nonnull stream, char* _Nonnull tags);

#ifdef __cplusplus
}
#endif

#endif //SYSTEM_AAUDIO_H
