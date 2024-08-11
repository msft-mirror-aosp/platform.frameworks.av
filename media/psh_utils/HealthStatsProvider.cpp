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

#include "PowerStatsProvider.h"
#include <aidl/android/hardware/health/IHealth.h>
#include <android-base/logging.h>
#include <android/binder_manager.h>

using ::aidl::android::hardware::health::HealthInfo;
using ::aidl::android::hardware::health::IHealth;

namespace android::media::psh_utils {

static auto getHealthService() {
    [[clang::no_destroy]] static constinit std::mutex m;
    [[clang::no_destroy]] static constinit
            std::shared_ptr<IHealth> healthService;

    std::lock_guard l(m);
    if (healthService) {
        return healthService;
    }
    const auto serviceName =
            std::string(IHealth::descriptor).append("/default");
    healthService = IHealth::fromBinder(
            ::ndk::SpAIBinder(AServiceManager_checkService(serviceName.c_str())));
    return healthService;
}

status_t HealthStatsDataProvider::fill(PowerStats* stat) const {
    if (stat == nullptr) return BAD_VALUE;
    HealthStats& stats = stat->health_stats;
    auto healthService = getHealthService();
    if (healthService == nullptr) {
        LOG(ERROR) << "unable to get health AIDL service";
        return NO_INIT;
    }
    HealthInfo healthInfo;
    if (!healthService->getHealthInfo(&healthInfo).isOk()) {
        LOG(ERROR) << __func__ << ": unable to get health info";
        return INVALID_OPERATION;
    }

    stats.batteryVoltageMillivolts = healthInfo.batteryVoltageMillivolts;
    stats.batteryFullChargeUah = healthInfo.batteryFullChargeUah;
    stats.batteryChargeCounterUah = healthInfo.batteryChargeCounterUah;
    return NO_ERROR;
}

} // namespace android::media::psh_utils
