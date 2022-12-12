package com.dolphin.benchmark

import androidx.benchmark.macro.ExperimentalBaselineProfilesApi
import androidx.benchmark.macro.junit4.BaselineProfileRule
import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import androidx.test.uiautomator.UiSelector
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@OptIn(ExperimentalBaselineProfilesApi::class)
@RunWith(AndroidJUnit4ClassRunner::class)
class BaselineProfileGenerator {

    @get:Rule
    val rule = BaselineProfileRule()

    @Test
    fun generate() = rule.collectBaselineProfile(
        packageName = "org.dolphinemu.dolphinemu.benchmark",
        profileBlock = {
            pressHome()
            startActivityAndWait()

            // Dismiss analytics dialog due to issue with finding button
            device.pressBack()

            // Navigate through activities that don't require games
            // TODO: Make all activities testable without having games available
            // TODO: Use resource strings to support more languages

            // Navigate to the Settings Activity
            val settings = device.findObject(UiSelector().description("Settings"))
            settings.clickAndWaitForNewWindow(30_000)

            // Go through settings and to the User Data Activity
            val config = device.findObject(UiSelector().textContains("Config"))
            config.click()
            val userData = device.findObject(UiSelector().textContains("User Data"))
            userData.clickAndWaitForNewWindow(30_000)
        },
    )
}
