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

// PCM offload

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include <aaudio/AAudio.h>

#include "AAudioArgsParser.h"
#include "AAudioSimplePlayer.h"
#include "SineGenerator.h"

const static int DEFAULT_TIME_TO_RUN_IN_SECOND = 5;

aaudio_data_callback_result_t MyDatacallback(AAudioStream* stream,
                                             void* userData,
                                             void* audioData,
                                             int32_t numFrames);

void MyErrorCallback(AAudioStream* /*stream*/, void* /*userData*/, aaudio_result_t error);

class OffloadPlayer : public AAudioSimplePlayer {
public:
    OffloadPlayer(AAudioArgsParser& argParser, int delay, int padding, int streamFrames)
            : mArgParser(argParser), mDelay(delay), mPadding(padding), mStreamFrames(streamFrames) {
    }

    aaudio_result_t open() {
        aaudio_result_t result = AAudioSimplePlayer::open(mArgParser,
                                                          &MyDatacallback,
                                                          &MyErrorCallback,
                                                          this);
        if (result != AAUDIO_OK) {
            return result;
        }
        mChannelCount = getChannelCount();
        for (int i = 0; i < mChannelCount; ++i) {
            SineGenerator sine;
            sine.setup(440.0, 48000.0);
            mSines.push_back(sine);
        }
        return result;
    }

    aaudio_data_callback_result_t renderAudio(AAudioStream* stream,
                                              void* audioData,
                                              int32_t numFrames) {
        // Just handle PCM_16 and PCM_FLOAT for testing
        switch (AAudioStream_getFormat(stream)) {
            case AAUDIO_FORMAT_PCM_I16: {
                int16_t *audioBuffer = static_cast<int16_t *>(audioData);
                for (int i = 0; i < mChannelCount; ++i) {
                    mSines[i].render(&audioBuffer[i], mChannelCount, numFrames);
                }
            } break;
            case AAUDIO_FORMAT_PCM_FLOAT: {
                float *audioBuffer = static_cast<float *>(audioData);
                for (int i = 0; i < mChannelCount; ++i) {
                    mSines[i].render(&audioBuffer[i], mChannelCount, numFrames);
                }
            } break;
            default:
                return AAUDIO_CALLBACK_RESULT_STOP;
        }
        mFramesWritten += numFrames;
        if (mStreamFrames > 0 && mFramesWritten >= mStreamFrames) {
            if (auto result = setOffloadEndOfStream(); result != AAUDIO_OK) {
                printf("Failed to set offload end of stream, stopping the stream now");
                return AAUDIO_CALLBACK_RESULT_STOP;
            }
            (void) setOffloadDelayPadding(mDelay, mPadding);
            mFramesWritten = 0;
        }
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

private:
    const AAudioArgsParser mArgParser;
    const int mDelay;
    const int mPadding;
    const int mStreamFrames;

    int mChannelCount;
    std::vector<SineGenerator> mSines;
    int mFramesWritten = 0;
};

aaudio_data_callback_result_t MyDatacallback(AAudioStream* stream,
                                             void* userData,
                                             void* audioData,
                                             int32_t numFrames) {
    OffloadPlayer* player = static_cast<OffloadPlayer*>(userData);
    return player->renderAudio(stream, audioData, numFrames);
}

void MyErrorCallback(AAudioStream* /*stream*/, void* /*userData*/, aaudio_result_t error) {
    printf("Error callback, error=%d\n", error);
}

static void usage() {
    AAudioArgsParser::usage();
    printf("      -D{delay} offload delay in frames\n");
    printf("      -P{padding} offload padding in frames\n");
    printf("      -E{frames} frames to notify end of stream\n");
    printf("      -T{seconds} time to run the test\n");
}

int main(int argc, char **argv) {
    AAudioArgsParser argParser;
    int delay = 0;
    int padding = 0;
    int streamFrames = 0;
    int timeToRun = DEFAULT_TIME_TO_RUN_IN_SECOND;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (argParser.parseArg(arg)) {
            if (arg[0] == '-') {
                char option = arg[1];
                switch (option) {
                    case 'D':
                        delay = atoi(&arg[2]);
                        break;
                    case 'P':
                        padding = atoi(&arg[2]);
                        break;
                    case 'E':
                        streamFrames = atoi(&arg[2]);
                        break;
                    case 'T':
                        timeToRun = atoi(&arg[2]);
                        break;
                    default:
                        usage();
                        exit(EXIT_FAILURE);
                }
            } else {
                usage();
                exit(EXIT_FAILURE);
            }
        }
    }

    // Force to use offload mode
    argParser.setPerformanceMode(AAUDIO_PERFORMANCE_MODE_POWER_SAVING_OFFLOADED);

    OffloadPlayer player(argParser, delay, padding, streamFrames);
    if (auto result = player.open(); result != AAUDIO_OK) {
        printf("Failed to open stream, error=%d\n", result);
        exit(EXIT_FAILURE);
    }

    // Failed to set offload delay and padding will affect the gapless transition between tracks
    // but doesn't affect playback.
    (void) player.setOffloadDelayPadding(delay, padding);

    if (auto result = player.start(); result != AAUDIO_OK) {
        printf("Failed to start stream, error=%d", result);
        exit(EXIT_FAILURE);
    }

    sleep(timeToRun);

    return EXIT_SUCCESS;
}
