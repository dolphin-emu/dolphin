// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.baselineprofile

import androidx.benchmark.macro.junit4.BaselineProfileRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import androidx.test.uiautomator.UiSelector
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@LargeTest
class BaselineProfileGenerator {

    @get:Rule
    val rule = BaselineProfileRule()

    @Test
    fun generate() {
        rule.collect("org.dolphinemu.dolphinemu") {
            pressHome()
            startActivityAndWait()

            // Dismiss analytics dialog due to issue with finding button
            device.pressBack()

            // Navigate through activities that don't require games
            // TODO: Make all activities testable without having games available (or use homebrew)
            // TODO: Use resource strings to support more languages

            // Navigate to the Settings Activity
            val settings = device.findObject(UiSelector().description("Settings"))
            settings.clickAndWaitForNewWindow(30_000)

            // Go through settings and to the User Data Activity
            val config = device.findObject(UiSelector().textContains("Config"))
            config.click()
            val userData = device.findObject(UiSelector().textContains("User Data"))
            userData.clickAndWaitForNewWindow(30_000)
        }
    }
}
