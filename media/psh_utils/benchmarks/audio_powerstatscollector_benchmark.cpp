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

#define LOG_TAG "audio_token_benchmark"
#include <utils/Log.h>

#include <psh_utils/PowerStatsCollector.h>

#include <benchmark/benchmark.h>

/*
 Pixel 8 Pro
------------------------------------------------------------------------------------------
 Benchmark                            Time                      CPU             Iteration
------------------------------------------------------------------------------------------
audio_powerstatscollector_benchmark:
  #BM_StatsToleranceMs/0      1.2005660120994434E8 ns            2532739.72 ns          100
  #BM_StatsToleranceMs/50        1281.095987079007 ns     346.0322183913503 ns      2022168
  #BM_StatsToleranceMs/100       459.9668862534226 ns    189.47902626735942 ns      2891307
  #BM_StatsToleranceMs/200       233.8438662484292 ns    149.84041813854736 ns      4407343
  #BM_StatsToleranceMs/500      184.42197142314103 ns    144.86896036787098 ns      7295167
*/

// We check how expensive it is to query stats depending
// on the tolerance to reuse the cached values.
// A tolerance of 0 means we always fetch stats.
static void BM_StatsToleranceMs(benchmark::State& state) {
    auto& collector = android::media::psh_utils::PowerStatsCollector::getCollector();
    const int64_t toleranceNs = state.range(0) * 1'000'000;
    while (state.KeepRunning()) {
        collector.getStats(toleranceNs);
        benchmark::ClobberMemory();
    }
}

// Here we test various time tolerances (given in milliseconds here)
BENCHMARK(BM_StatsToleranceMs)->Arg(0)->Arg(50)->Arg(100)->Arg(200)->Arg(500);

BENCHMARK_MAIN();
