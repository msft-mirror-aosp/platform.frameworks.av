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
#include <android-base/logging.h>
#include <psh_utils/PowerStatsCollector.h>
#include "PowerStatsProvider.h"
#include <utils/Timers.h>

namespace android::media::psh_utils {

PowerStatsCollector::PowerStatsCollector() {
    addProvider(std::make_unique<PowerEntityResidencyDataProvider>());
    addProvider(std::make_unique<RailEnergyDataProvider>());
}

/* static */
PowerStatsCollector& PowerStatsCollector::getCollector() {
    [[clang::no_destroy]] static PowerStatsCollector psc;
    return psc;
}

std::shared_ptr<PowerStats> PowerStatsCollector::getStats() const {
    auto result = std::make_shared<PowerStats>();
    (void)fill(result.get());
    return result;
}

void PowerStatsCollector::addProvider(std::unique_ptr<PowerStatsProvider>&& powerStatsProvider) {
    mPowerStatsProviders.emplace_back(std::move(powerStatsProvider));
}

int PowerStatsCollector::fill(PowerStats* stats) const {
    if (!stats) {
        LOG(ERROR) << __func__ << ": bad args; stat is null";
        return 1;
    }

    for (const auto& provider : mPowerStatsProviders) {
        if (provider->fill(stats) != 0) {
            LOG(ERROR) << __func__ << ": a data provider failed";
            continue;
        }
    }

    // boot time follows wall clock time, but starts from boot.
    stats->metadata.start_time_since_boot_ms = systemTime(SYSTEM_TIME_BOOTTIME) / 1'000'000;

    // wall clock time
    stats->metadata.start_time_epoch_ms = systemTime(SYSTEM_TIME_REALTIME) / 1'000'000;

    // monotonic time follows boot time, but does not include any time suspended.
    stats->metadata.start_time_monotonic_ms = systemTime() / 1'000'000;
    return 0;
}

} // namespace android::media::psh_utils
