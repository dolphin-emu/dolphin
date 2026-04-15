// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.ui.theme

import android.content.Context
import androidx.annotation.AttrRes
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.google.android.material.color.MaterialColors
import androidx.appcompat.R as AppCompatR
import com.google.android.material.R as MaterialR

object DolphinTheme {
    val scaffoldPadding = 16.dp
}

@Composable
fun DolphinTheme(content: @Composable () -> Unit) {
    val context = LocalContext.current
    val isDark = isSystemInDarkTheme()
    val colorScheme = remember(context, isDark) { context.toDolphinColorScheme(isDark) }

    MaterialTheme(
        colorScheme = colorScheme,
        content = content
    )
}

private fun Context.toDolphinColorScheme(isDark: Boolean): ColorScheme {
    fun attr(@AttrRes attr: Int) = Color(MaterialColors.getColor(this, attr, 0))

    val background = obtainStyledAttributes(intArrayOf(android.R.attr.colorBackground)).use {
        Color(it.getColor(0, 0))
    }

    return if (isDark) {
        darkColorScheme(
            primary = attr(AppCompatR.attr.colorPrimary),
            onPrimary = attr(MaterialR.attr.colorOnPrimary),
            primaryContainer = attr(MaterialR.attr.colorPrimaryContainer),
            onPrimaryContainer = attr(MaterialR.attr.colorOnPrimaryContainer),
            secondary = attr(MaterialR.attr.colorSecondary),
            onSecondary = attr(MaterialR.attr.colorOnSecondary),
            secondaryContainer = attr(MaterialR.attr.colorSecondaryContainer),
            onSecondaryContainer = attr(MaterialR.attr.colorOnSecondaryContainer),
            tertiary = attr(MaterialR.attr.colorTertiary),
            onTertiary = attr(MaterialR.attr.colorOnTertiary),
            tertiaryContainer = attr(MaterialR.attr.colorTertiaryContainer),
            onTertiaryContainer = attr(MaterialR.attr.colorOnTertiaryContainer),
            error = attr(AppCompatR.attr.colorError),
            onError = attr(MaterialR.attr.colorOnError),
            errorContainer = attr(MaterialR.attr.colorErrorContainer),
            onErrorContainer = attr(MaterialR.attr.colorOnErrorContainer),
            background = background,
            onBackground = attr(MaterialR.attr.colorOnBackground),
            surface = attr(MaterialR.attr.colorSurface),
            onSurface = attr(MaterialR.attr.colorOnSurface),
            surfaceVariant = attr(MaterialR.attr.colorSurfaceVariant),
            onSurfaceVariant = attr(MaterialR.attr.colorOnSurfaceVariant),
            outline = attr(MaterialR.attr.colorOutline),
            inverseSurface = attr(MaterialR.attr.colorSurfaceInverse),
            inverseOnSurface = attr(MaterialR.attr.colorOnSurfaceInverse),
            inversePrimary = attr(MaterialR.attr.colorPrimaryInverse),
        )
    } else {
        lightColorScheme(
            primary = attr(AppCompatR.attr.colorPrimary),
            onPrimary = attr(MaterialR.attr.colorOnPrimary),
            primaryContainer = attr(MaterialR.attr.colorPrimaryContainer),
            onPrimaryContainer = attr(MaterialR.attr.colorOnPrimaryContainer),
            secondary = attr(MaterialR.attr.colorSecondary),
            onSecondary = attr(MaterialR.attr.colorOnSecondary),
            secondaryContainer = attr(MaterialR.attr.colorSecondaryContainer),
            onSecondaryContainer = attr(MaterialR.attr.colorOnSecondaryContainer),
            tertiary = attr(MaterialR.attr.colorTertiary),
            onTertiary = attr(MaterialR.attr.colorOnTertiary),
            tertiaryContainer = attr(MaterialR.attr.colorTertiaryContainer),
            onTertiaryContainer = attr(MaterialR.attr.colorOnTertiaryContainer),
            error = attr(AppCompatR.attr.colorError),
            onError = attr(MaterialR.attr.colorOnError),
            errorContainer = attr(MaterialR.attr.colorErrorContainer),
            onErrorContainer = attr(MaterialR.attr.colorOnErrorContainer),
            background = background,
            onBackground = attr(MaterialR.attr.colorOnBackground),
            surface = attr(MaterialR.attr.colorSurface),
            onSurface = attr(MaterialR.attr.colorOnSurface),
            surfaceVariant = attr(MaterialR.attr.colorSurfaceVariant),
            onSurfaceVariant = attr(MaterialR.attr.colorOnSurfaceVariant),
            outline = attr(MaterialR.attr.colorOutline),
            inverseSurface = attr(MaterialR.attr.colorSurfaceInverse),
            inverseOnSurface = attr(MaterialR.attr.colorOnSurfaceInverse),
            inversePrimary = attr(MaterialR.attr.colorPrimaryInverse),
        )
    }
}

@Composable
fun MenuSpacer() = Spacer(modifier = Modifier.height(16.dp))
