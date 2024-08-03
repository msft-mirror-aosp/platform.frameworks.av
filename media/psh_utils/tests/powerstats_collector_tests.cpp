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

#include <psh_utils/PowerStatsCollector.h>
#include <gtest/gtest.h>
#include <utils/Log.h>

TEST(powerstat_collector_tests, basic) {
    const auto& psc = android::media::psh_utils::PowerStatsCollector::getCollector();

    // This test is used for debugging the string through logcat, we validate a non-empty string.
    auto powerStats = psc.getStats();
    ALOGD("%s: %s", __func__, powerStats->toString().c_str());
    EXPECT_FALSE(powerStats->toString().empty());
}

TEST(powerstat_collector_tests, arithmetic) {
    android::media::psh_utils::PowerStats ps1, ps2;
    ps1.metadata.duration_ms = 5;
    ps2.metadata.duration_ms = 10;

    // duration follows add / subtract rules.
    android::media::psh_utils::PowerStats ps3 = ps1 + ps2;
    android::media::psh_utils::PowerStats ps4 = ps2 + ps1;
    EXPECT_EQ(ps3, ps4);
    EXPECT_EQ(15, ps3.metadata.duration_ms);

    android::media::psh_utils::PowerStats ps5 = ps2 - ps1;
    android::media::psh_utils::PowerStats ps6 = ps5 + ps1;
    EXPECT_EQ(ps6, ps2);
    EXPECT_EQ(5, ps5.metadata.duration_ms);
}
