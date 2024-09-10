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

#define LOG_TAG "audio_powerstat_benchmark"
#include <cutils/properties.h>
#include <utils/Log.h>

#include <psh_utils/PerformanceFixture.h>

#include <algorithm>
#include <android-base/strings.h>
#include <random>
#include <thread>
#include <vector>

float result = 0;

using android::media::psh_utils::CoreClass;
using android::media::psh_utils::CORE_LITTLE;
using android::media::psh_utils::CORE_MID;
using android::media::psh_utils::CORE_BIG;

enum Direction {
    DIRECTION_FORWARD,
    DIRECTION_BACKWARD,
    DIRECTION_RANDOM,
};

std::string toString(Direction direction) {
    switch (direction) {
        case DIRECTION_FORWARD: return "DIRECTION_FORWARD";
        case DIRECTION_BACKWARD: return "DIRECTION_BACKWARD";
        case DIRECTION_RANDOM: return "DIRECTION_RANDOM";
        default: return "DIRECTION_UNKNOWN";
    }
}

enum Content {
    CONTENT_ZERO,
    CONTENT_RANDOM,
};

std::string toString(Content content) {
    switch (content) {
        case CONTENT_ZERO: return "CONTENT_ZERO";
        case CONTENT_RANDOM: return "CONTENT_RANDOM";
        default: return "CONTENT_UNKNOWN";
    }
}

class MemoryFixture : public android::media::psh_utils::PerformanceFixture {
public:
    void SetUp(benchmark::State& state) override {
        mCount = state.range(0) / (sizeof(uint32_t) + sizeof(float));
        state.SetComplexityN(mCount * 2);  // 2 access per iteration.

        // create src distribution
        mSource.resize(mCount);
        const auto content = static_cast<Content>(state.range(3));
        if (content == CONTENT_RANDOM) {
            std::minstd_rand gen(mCount);
            std::uniform_real_distribution<float> dis(-1.f, 1.f);
            for (size_t i = 0; i < mCount; i++) {
                mSource[i] = dis(gen);
            }
        }

        // create direction
        mIndex.resize(mCount);
        const auto direction = static_cast<Direction>(state.range(2));
        switch (direction) {
            case DIRECTION_BACKWARD:
                for (size_t i = 0; i < mCount; i++) {
                    mIndex[i] = i;  // it is also possible to go in the reverse direction
                }
                break;
            case DIRECTION_FORWARD:
            case DIRECTION_RANDOM:
                for (size_t i = 0; i < mCount; i++) {
                    mIndex[i] = i;  // it is also possible to go in the reverse direction
                }
                if (direction == DIRECTION_RANDOM) {
                    std::random_device rd;
                    std::mt19937 g(rd());
                    std::shuffle(mIndex.begin(), mIndex.end(), g);
                }
                break;
        }

        // set up the profiler
        const auto coreClass = static_cast<CoreClass>(state.range(1));

        // It would be best if we could override SetName() but it is too late at this point,
        // so we set the state label here for clarity.
        state.SetLabel(toString(coreClass).append("/")
            .append(toString(direction)).append("/")
            .append(toString(content)));

        if (property_get_bool("persist.audio.benchmark_profile", false)) {
            startProfiler(coreClass);
        }
    }
    size_t mCount = 0;
    std::vector<uint32_t> mIndex;
    std::vector<float> mSource;
};

BENCHMARK_DEFINE_F(MemoryFixture, CacheAccess)(benchmark::State &state) {
    float accum = 0;
    while (state.KeepRunning()) {
        for (size_t i = 0; i < mCount; ++i) {
            accum += mSource[mIndex[i]];
        }
        benchmark::ClobberMemory();
    }
    result += accum; // not optimized
}

BENCHMARK_REGISTER_F(MemoryFixture, CacheAccess)->ArgsProduct({
    benchmark::CreateRange(64, 64<<20, /* multi = */2),
    {CORE_LITTLE, CORE_MID, CORE_BIG},
    {DIRECTION_FORWARD, DIRECTION_RANDOM},
    {CONTENT_RANDOM},
});

BENCHMARK_MAIN();
