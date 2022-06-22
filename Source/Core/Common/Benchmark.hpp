// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <numeric>
#include <string>

#include "Common/Logging/Log.h"
/*
  Tutorial

    Adding a new benchmark

      1) Add a descriptive name for your benchmark into the Benchmarks enum.
      2) Add {[YOUR_BENCHMARK_NAME], false/true}, into the initializer list for Benchmarks_Enabled.
      3) Include "Common/Benchmark.hpp" into the file which contains the code you want to benchmark.
      4) Put the following code around the code you would like to benchmark.
*/
// clang-format off
/*
if constexpr (Benchmark::Benchmarks_Enabled.at(Benchmark::Benchmarks::[YOUR_BENCHMARK_NAME]).enabled)
  Benchmark::BenchmarkStart("[YOUR_BENCHMARK_NAME_IN_LOG]");

[CODE YOU WANT BENCHMARKED]

if constexpr (Benchmark::Benchmarks_Enabled.at(Benchmark::Benchmarks::[YOUR_BENCHMARK_NAME]).enabled)
  Benchmark::BenchmarkEnd();
*/
// clang-format on
/*

    Creating Temporary Benchmarks

      1) Include "Common/Benchmark.hpp" in the file with the code you want to benchmark.
      2) Put Benchmark::BenchmarkStart("[BENCHMARK_NAME_IN_LOG]"); before the code you would like to
         benchmark.
      3) Put Benchmark::BenchmarkEnd(); after the code you would like to benchmark.
      4) Take your measurements.
      5) Delete your benchmark by removing the code added in the previous steps.

    EX. 1 Measures JitCompileLoop()

      Benchmark::BenchmarkStart("Jit Compile Loop");
      JitCompileLoop();
      Benchmark::BenchmarkEnd();

    EX. 2 Measures the time between JitCompileLoop() calls

      Benchmark::BenchmarkEnd();
      JitCompileLoop();
      Benchmark::BenchmarkStart("Time between JitCompileLoop() calls");


    Disabling/Enabling Benchmarks

      1) Set the bool that corresponds with your benchmark's enum in the Benchmarks_Enabled array
         to true to enable and false to disable.

    How to view your results

       Every time the code being benchmarked is called, the benchmark will post a message under the
   BENCHMARK tag in the in-app Dolphin log. This post includes the number of samples, current
   measurement, average(mean) measurement, and maximum measurement. A sample is when both
   BenchmarkStart() and BenchmarkEnd() have been called.

    Extra Info and Recommendations

        Disabled benchmarks will have zero runtime overhead because they will not be compiled. Make
   sure to turn off all of the benchmarks for any distribution build. I recommend turning all of
   the benchmarks off except the one you're actively using to make your results as accurate as
   possible. I don't believe these tests are thread-safe. They are intended to only be used one at a
   time and in one thread at a time.
*/
namespace Benchmark
{
enum Benchmarks
{
  /*[YOUR_BENCHMARK_NAME],*/
  JIT_Translation,
  BENCHMARK_COUNT
};

struct BenchmarkState
{
  Benchmarks benchmark;
  bool enabled;
};

// Make sure all of these are turned off for any distribution builds
constexpr std::array<BenchmarkState, BENCHMARK_COUNT + 1> Benchmarks_Enabled{
    /*{[YOUR_BENCHMARK_NAME], true/false}*/
    {JIT_Translation, true},
};

static std::string text_to_log;
[[maybe_unused]] static void DyanmicBenchmarkLog(const std::string& _text_to_log)
{
  text_to_log = _text_to_log;
  GenericLogFmtImpl(Common::Log::LogLevel::LINFO, Common::Log::LogType::BENCHMARK, __FILE__,
                    __LINE__, FMT_STRING(text_to_log.c_str()), fmt::format_args{});
}

static std::chrono::steady_clock::time_point start_time;
static std::string benchmark_name{};

[[maybe_unused]] static void BenchmarkStart(const std::string& _benchmark_name)
{
  benchmark_name = _benchmark_name;
  start_time = std::chrono::steady_clock::now();
}

[[maybe_unused]] static void BenchmarkEnd()
{
  std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();

  static std::vector<std::chrono::steady_clock::time_point::rep> measurements(256000);
  static std::chrono::steady_clock::time_point::rep max_measurement = 0;

  std::chrono::nanoseconds current_measurement =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

  max_measurement = std::max(max_measurement, current_measurement.count());

  measurements.push_back(current_measurement.count());
  double average_measurement =
      std::accumulate(measurements.begin(), measurements.end(), 0.0) / measurements.size();

  constexpr static std::string::size_type tab_spaces = 6;
  const std::string tab(tab_spaces, ' ');
  const std::string new_line{'\n'};
  // clang-format off
  DyanmicBenchmarkLog(new_line + std::string{"Benchmark - "} + benchmark_name
    + new_line + std::string{"-------------------------------"} 
    + new_line + tab + std::string{"Current(ns): "} + std::to_string(current_measurement.count())
    + new_line + tab + std::string{"Max(ns): "} + std::to_string(max_measurement)
    + new_line + tab + std::string{"Samples: "} + std::to_string(measurements.size())
    + new_line + tab + std::string{"Average(Mean)(ns): "} + std::to_string(average_measurement));
  // clang-format on
}
}  // namespace Benchmark
