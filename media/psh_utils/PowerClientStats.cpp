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

#include <psh_utils/PowerClientStats.h>
#include <mediautils/ServiceUtilities.h>

namespace android::media::psh_utils {

/* static */
audio_utils::CommandThread& PowerClientStats::getCommandThread() {
    [[clang::no_destroy]] static audio_utils::CommandThread ct;
    return ct;
}

PowerClientStats::PowerClientStats(uid_t uid, const std::string& additional)
        : mUid(uid), mAdditional(additional) {}

void PowerClientStats::start(int64_t actualNs) {
    std::lock_guard l(mMutex);
    ++mTokenCount;
    if (mStartNs == 0) mStartNs = actualNs;
    if (mStartStats) return;
    mStartStats = PowerStatsCollector::getCollector().getStats(kStatTimeToleranceNs);
}

void PowerClientStats::stop(int64_t actualNs) {
    std::lock_guard l(mMutex);
    if (--mTokenCount > 0) return;
    if (mStartNs != 0) mCumulativeNs += actualNs - mStartNs;
    mStartNs = 0;
    if (!mStartStats) return;
    const auto stopStats = PowerStatsCollector::getCollector().getStats(kStatTimeToleranceNs);
    if (stopStats && stopStats != mStartStats) {
        *mCumulativeStats += *stopStats - *mStartStats;
    }
    mStartStats.reset();
}

void PowerClientStats::addPid(pid_t pid) {
    std::lock_guard l(mMutex);
    mPids.emplace(pid);
}

size_t PowerClientStats::removePid(pid_t pid) {
    std::lock_guard l(mMutex);
    mPids.erase(pid);
    return mPids.size();
}

std::string PowerClientStats::toString(bool stats, const std::string& prefix) const {
    std::lock_guard l(mMutex);

    // Adjust delta time and stats if currently running.
    auto cumulativeStats = mCumulativeStats;
    auto cumulativeNs = mCumulativeNs;
    if (mStartNs) cumulativeNs += systemTime(SYSTEM_TIME_BOOTTIME) - mStartNs;
    if (mStartStats) {
        const auto stopStats = PowerStatsCollector::getCollector().getStats(kStatTimeToleranceNs);
        if (stopStats && stopStats != mStartStats) {
            auto newStats = std::make_shared<PowerStats>(*cumulativeStats);
            *newStats += *stopStats - *mStartStats;
            cumulativeStats = newStats;
        }
    }

    std::string result(prefix);
    result.append("uid: ")
            .append(std::to_string(mUid))
            .append(" ").append(mediautils::UidInfo::getInfo(mUid)->package)
            .append(" streams: ").append(std::to_string(mTokenCount))
            .append(" seconds: ").append(std::to_string(cumulativeNs * 1e-9));
    result.append(" {");
    for (auto pid : mPids) {
        result.append(" ").append(std::to_string(pid));
    }
    result.append(" }");
    if (!mAdditional.empty()) {
        result.append("\n").append(prefix).append(mAdditional);
    }
    if (stats) {
        std::string prefix2(prefix);
        prefix2.append("  ");
        result.append("\n").append(cumulativeStats->normalizedEnergy(prefix2));
    }
    return result;
}

} // namespace android::media::psh_utils
