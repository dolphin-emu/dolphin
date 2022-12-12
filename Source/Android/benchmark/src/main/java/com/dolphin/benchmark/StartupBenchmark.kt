package com.dolphin.benchmark

import androidx.benchmark.macro.BaselineProfileMode
import androidx.benchmark.macro.CompilationMode
import androidx.benchmark.macro.StartupMode
import androidx.benchmark.macro.StartupTimingMetric
import androidx.benchmark.macro.junit4.MacrobenchmarkRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class StartupBenchmark {
    @get:Rule
    val benchmarkRule = MacrobenchmarkRule()

    @Test
    fun startupBaselineProfileDisabled() = startup(
        CompilationMode.Partial(baselineProfileMode = BaselineProfileMode.Disable, warmupIterations = 1)
    )

    @Test
    fun startupBaselineProfile() = startup(
        CompilationMode.Partial(baselineProfileMode = BaselineProfileMode.Require, warmupIterations = 1)
    )

    private fun startup(compilationMode: CompilationMode) = benchmarkRule.measureRepeated(
        packageName = "org.dolphinemu.dolphinemu.benchmark",
        metrics = listOf(StartupTimingMetric()),
        iterations = 10,
        compilationMode = compilationMode,
        startupMode = StartupMode.COLD,
        setupBlock = {
            pressHome()
        }
    ) {
        startActivityAndWait()
    }
}
