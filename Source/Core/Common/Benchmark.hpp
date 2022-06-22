// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <iterator>
#include <mutex>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "Common/Logging/Log.h"
/*
  Tutorial

    Adding a new benchmark

      1) Add a descriptive name for your benchmark into the Benchmarks enum.
      2) Add {false/true, [YOUR_BENCHMARK_NAME]}, into the initializer list for Benchmarks_Enabled.
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

struct BenchmarkControl
{
  bool enabled;
  bool print_to_log;
  bool display_on_screen;
  Benchmarks benchmark;
};

// Make sure all of these are disabled for any distribution builds
constexpr std::array<BenchmarkControl, BENCHMARK_COUNT + 1> Benchmarks_Enabled{
    /*{true/false, [YOUR_BENCHMARK_NAME]}*/
    {true, true, true, JIT_Translation},
};

template <typename Duration, typename TimePoint>
struct Benchmark
{
  std::string name{};
  std::vector<Duration> samples{};
  TimePoint start{};
  TimePoint end{};
  Duration this_run{};
  Duration max{};
  Duration min{};
  Duration mean{};
};

static std::string text_to_log;
[[maybe_unused]] static void DyanmicBenchmarkLog(const std::string& _text_to_log)
{
  text_to_log = _text_to_log;
  GenericLogFmtImpl(Common::Log::LogLevel::LINFO, Common::Log::LogType::BENCHMARK, __FILE__,
                    __LINE__, {text_to_log.c_str()}, fmt::format_args{});
}

using clock = std::chrono::steady_clock;

static std::mutex running_benchmarks_mutex;
static std::vector<Benchmark<clock::duration, clock::time_point>> running_benchmarks{};

[[maybe_unused]] static void BenchmarkStart(const std::string& benchmark_name)
{
  std::lock_guard<std::mutex> lock(running_benchmarks_mutex);
  running_benchmarks.push_back(Benchmark<clock::duration, clock::time_point>{benchmark_name});
  std::prev(running_benchmarks.end())->start = clock::now();
}

constexpr static std::string::size_type tab_spaces = 6;
const std::string tab(tab_spaces, ' ');
const std::string new_line{'\n'};

[[maybe_unused]] static void BenchmarkEnd(const std::string& benchmark_name)
{
  const auto end = clock::now();
  std::lock_guard<std::mutex> lock(running_benchmarks_mutex);

  std::vector<Benchmark<clock::duration, clock::time_point>>::iterator working_benchmark;
  for (auto benchmark = running_benchmarks.begin(); benchmark != running_benchmarks.end();
       std::advance(benchmark, 1))
  {
    if (benchmark->name == benchmark_name)
    {
      working_benchmark = benchmark;
      //running_benchmarks.erase(benchmark);
      break;
    }
    else if (benchmark == std::prev(running_benchmarks.end()))
    {
      DyanmicBenchmarkLog(std::string{"Benchmark: "} + benchmark_name +
                          std::string{" does not exist."});
      return;
    }
  }

  working_benchmark.this_run = working_benchmark.end - working_benchmark.start;
  working_benchmark.max = std::max(working_benchmark.this_run, working_benchmark.max);
  working_benchmark.min = std::max(working_benchmark.this_run, working_benchmark.min);
  working_benchmark.samples.push_back(working_benchmark.this_run);
  working_benchmark.mean = std::accumulate(working_benchmark.samples.begin(),
                                           working_benchmark.samples.end(), clock::duration{}) /
      working_benchmark.samples.size();

  // clang-format off
  DyanmicBenchmarkLog(new_line + std::string{"Benchmark - "} + working_benchmark.name
    + new_line + std::string{"-------------------------------"} 
    + new_line + tab + std::string{"Samples: "} + std::to_string(working_benchmark.samples.size())
    + new_line + tab + std::string{"Average(Mean)(ns): "} + std::to_string(working_benchmark.mean.count())
    + new_line + tab + std::string{"This Run(ns): "} + std::to_string(working_benchmark.this_run.count())
    + new_line + tab + std::string{"Min(ns): "} + std::to_string(working_benchmark.min.count())
    + new_line + tab + std::string{"Max(ns): "} + std::to_string(working_benchmark.max.count()));
  // clang-format on
}
}  // namespace Benchmark
