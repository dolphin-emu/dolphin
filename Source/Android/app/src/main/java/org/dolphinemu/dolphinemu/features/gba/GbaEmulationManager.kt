// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.gba

import android.content.Context
import android.content.res.Configuration
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.View
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.content.edit
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.ActivityEmulationBinding
import org.dolphinemu.dolphinemu.features.input.model.InputOverrider
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity
import org.dolphinemu.dolphinemu.overlay.InputOverlay
import org.dolphinemu.dolphinemu.R

class GbaEmulationManager(
    private val activity: EmulationActivity,
    private val binding: ActivityEmulationBinding
) {
    val gbaViews = mutableListOf<GbaOverlayView>()
    val lastGbaTapTimes = mutableMapOf<Int, Long>()
    var isGbaLocked = false
    private var isMenuOpen = false
    private var isWaitingForGcOffsetReset = false

    private var settings: Settings? = null
    private var onRefreshOverlay: (() -> Unit)? = null

    private val slotPrefs = (0 until 4).associateWith { slot ->
        activity.getSharedPreferences("$PREF_GBA_OVERLAY_GLOBAL$slot", Context.MODE_PRIVATE)
    }
    private val globalGbaPrefs =
        activity.getSharedPreferences(PREF_GBA_OVERLAY_GLOBAL, Context.MODE_PRIVATE)

    // Property to handle aspect ratio calculations consistently.
    private val GbaOverlayView.aspectRatio: Float
        get() = if (nativeWidth > 0 && nativeHeight > 0) {
            nativeWidth.toFloat() / nativeHeight.toFloat()
        } else {
            1.5f
        }

    fun initSettings(settings: Settings, onRefreshOverlay: () -> Unit) {
        this.settings = settings
        this.onRefreshOverlay = onRefreshOverlay
    }

    private fun activeGbaSlots() = (0..3).filter {
        IntSetting.getSettingForSIDevice(it).int == InputOverlay.EMULATED_GBA_CONTROLLER
    }

    fun initViews() {
        isGbaLocked = globalGbaPrefs.getBoolean(PREF_GBA_LOCKED, false)

        activity.isMenuShowing.observe(activity) { open ->
            if (open != isMenuOpen) {
                isMenuOpen = open
                if (!open && !isGbaLocked) reattachTouchListeners()
            }
        }

        activeGbaSlots().forEach { slot ->
            val view = GbaOverlayView(activity).apply {
                gbaSlot = slot
                onDimensionsChanged = { applyGbaLayout() }
                visibility = View.VISIBLE
            }

            val sp = slotPrefs.getValue(slot)
            val sw = sp.getFloat(PREF_GBA_WIDTH, DEFAULT_GBA_WIDTH)
                .coerceIn(GBA_MIN_WIDTH, GBA_MAX_WIDTH)
            val sh = sw / view.aspectRatio

            binding.root.addView(view, 0, FrameLayout.LayoutParams(sw.toInt(), sh.toInt()))
            InputOverrider.registerGba(slot)
            applyStoredGbaVolume(slot)
            attachGbaTouchListener(view, slot, sp)
            gbaViews.add(view)
        }

        if (gbaViews.isNotEmpty() && NativeLibrary.IsGameMetadataValid()) {
            GbaRenderer.attach(gbaViews)
        }
    }

    fun onTitleChanged() {
        val slots = activeGbaSlots()
        gbaViews.forEachIndexed { i, v ->
            if (i < slots.size) {
                v.gbaSlot = slots[i]
                v.visibility = View.VISIBLE
                InputOverrider.registerGba(slots[i])
            } else {
                v.visibility = View.GONE
            }
        }
        if (gbaViews.isNotEmpty()) {
            if (GbaRenderer.isAttached()) {
                GbaRenderer.updateViews(gbaViews)
            } else {
                GbaRenderer.attach(gbaViews)
            }
            binding.root.post { applyGbaLayout() }
        }
    }

    fun onDestroy() {
        GbaRenderer.detach()
        GbaRenderer.setTvLeftOffset(0)
        (0..3).forEach { InputOverrider.unregisterGba(it) }
        gbaViews.forEach { binding.root.removeView(it) }
        gbaViews.clear()
    }

    fun onConfigurationChanged() {
        if (gbaViews.isNotEmpty()) {
            GbaRenderer.updateViews(gbaViews)
            binding.root.post { applyGbaLayout() }
        }
    }

    fun handleTouch(event: MotionEvent): Boolean {
        if (isGbaLocked || isMenuOpen || gbaViews.isEmpty()) return false
        val loc = IntArray(2)
        return gbaViews.find { view ->
            view.getLocationOnScreen(loc)
            android.graphics.Rect(loc[0], loc[1], loc[0] + view.width, loc[1] + view.height)
                .contains(event.rawX.toInt(), event.rawY.toInt())
        }?.dispatchTouchEvent(event) ?: false
    }

    fun setGbaViewsTouchable(touchable: Boolean) {
        gbaViews.forEach { view ->
            view.isClickable = touchable
            view.isFocusable = touchable
            view.isFocusableInTouchMode = touchable
            if (!touchable) view.setOnTouchListener(null)
        }
        if (touchable) {
            reattachTouchListeners()
        } else {
            gbaViews.forEach { view -> view.setOnTouchListener(null) }
        }
    }

    fun reattachTouchListeners() {
        if (isGbaLocked) return
        gbaViews.forEach { view ->
            attachGbaTouchListener(view, view.gbaSlot, slotPrefs.getValue(view.gbaSlot))
        }
    }

    private fun attachGbaTouchListener(
        view: GbaOverlayView,
        slot: Int,
        sp: android.content.SharedPreferences
    ) {
        var dragX = 0f
        var dragY = 0f
        val scaleDetector = ScaleGestureDetector(
            activity,
            object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
                override fun onScale(d: ScaleGestureDetector): Boolean {
                    if (isGbaLocked) return true
                    val ratio = view.aspectRatio
                    val ow = view.width.toFloat()
                    val oh = view.height.toFloat()
                    val nw = (ow * d.scaleFactor).coerceIn(GBA_MIN_WIDTH, GBA_MAX_WIDTH)
                    val nh = nw / ratio
                    view.x += (ow - nw) / 2f; view.y += (oh - nh) / 2f
                    view.layoutParams = (view.layoutParams as FrameLayout.LayoutParams).apply {
                        width = nw.toInt(); height = nh.toInt()
                    }
                    sp.edit {
                        putFloat(PREF_GBA_WIDTH, nw)
                        putFloat(PREF_GBA_HEIGHT, nh)
                        putFloat(PREF_GBA_X, view.x)
                        putFloat(PREF_GBA_Y, view.y)
                    }
                    return true
                }
            })

        view.setOnTouchListener { v, event ->
            if (isGbaLocked) return@setOnTouchListener false
            scaleDetector.onTouchEvent(event)
            if (scaleDetector.isInProgress) return@setOnTouchListener true
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    dragX = event.rawX - v.x; dragY = event.rawY - v.y
                }

                MotionEvent.ACTION_MOVE -> {
                    v.x = event.rawX - dragX; v.y = event.rawY - dragY
                }

                MotionEvent.ACTION_UP -> {
                    v.performClick()
                    val now = System.currentTimeMillis()
                    if (now - (lastGbaTapTimes[slot] ?: 0L) < 300) view.onDoubleTap()
                    lastGbaTapTimes[slot] = now
                    sp.edit { putFloat(PREF_GBA_X, v.x); putFloat(PREF_GBA_Y, v.y) }
                }
            }
            true
        }
    }

    fun applyGbaLayout() {
        if (gbaViews.isEmpty()) return
        val rootWidth = binding.root.width
        val rootHeight = binding.root.height

        if (rootWidth <= 0 || rootHeight <= 0) {
            binding.root.post { applyGbaLayout() }
            return
        }

        // Wait for at least one frame to be presented so we have valid frame dimensions when locked.
        if (isGbaLocked && GbaRenderer.getFrameCount() < 1) {
            binding.root.postDelayed({ applyGbaLayout() }, 100)
            return
        }

        val isLandscape =
            activity.resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

        when {
            isGbaLocked && isLandscape -> applyLockedLandscapeLayout(rootWidth, rootHeight)
            isGbaLocked -> applyLockedPortraitLayout(rootWidth, rootHeight)
            else -> restoreUnlockedLayout()
        }

        globalGbaPrefs.edit {
            putBoolean(PREF_GBA_LOCKED, isGbaLocked)
        }
        activeGbaSlots().forEach { applyStoredGbaVolume(it) }
    }

    private fun applyLockedLandscapeLayout(rootWidth: Int, rootHeight: Int) {
        val count = gbaViews.size
        if (count == 0) return
        val slotHeight = rootHeight / count
        val gcDrawWidth = GbaRenderer.getTvDrawWidth()
        val totalBlackBarSpace = (rootWidth - gcDrawWidth).coerceAtLeast(0)
        val sidebarWidth = totalBlackBarSpace
        var maxActualGbaWidth = 0

        gbaViews.forEachIndexed { index, view ->
            view.isScreenVisible = true
            view.needsBorderRedraw = false
            val ratio = view.aspectRatio

            var tw = sidebarWidth
            var th = (tw / ratio).toInt()
            if (th > slotHeight) {
                th = slotHeight
                tw = (th * ratio).toInt()
            }

            if (tw > maxActualGbaWidth) maxActualGbaWidth = tw

            val x = 0f
            view.setBounds(tw, th, x, index * slotHeight + (slotHeight - th) / 2f)
        }

        GbaRenderer.setTvLeftOffset(maxActualGbaWidth)

        if (!isWaitingForGcOffsetReset) {
            isWaitingForGcOffsetReset = true
            binding.root.post { applyGbaLayout() }
        } else {
            isWaitingForGcOffsetReset = false
        }
    }

    private fun applyLockedPortraitLayout(rootWidth: Int, rootHeight: Int) {
        isWaitingForGcOffsetReset = false
        val count = gbaViews.size
        val gbaTop = GbaRenderer.getTvDrawTop() + GbaRenderer.getTvDrawHeight()
        val availH = rootHeight - gbaTop
        val columns = if (count <= 2) count else 2
        val rows = (count + columns - 1) / columns
        val sw = rootWidth / columns
        val sh = availH / rows

        gbaViews.forEachIndexed { index, view ->
            val ratio = view.aspectRatio
            val th = (sw / ratio).toInt().coerceAtMost(sh).coerceAtMost(400)
            val tw = (th * ratio).toInt()
            val col = index % columns
            val row = index / columns
            view.setBounds(tw, th, col * sw + (sw - tw) / 2f, gbaTop + row * sh + (sh - th) / 2f)
        }
    }

    private fun restoreUnlockedLayout() {
        isWaitingForGcOffsetReset = false
        gbaViews.forEachIndexed { index, view ->
            restoreViewFromPrefs(view, view.gbaSlot, index)
        }
    }

    private fun GbaOverlayView.setBounds(width: Int, height: Int, x: Float, y: Float) {
        setOnTouchListener(null)
        val params = layoutParams as FrameLayout.LayoutParams
        params.width = width
        params.height = height
        layoutParams = params
        this.x = x
        this.y = y
        visibility = View.VISIBLE
    }

    private fun restoreViewFromPrefs(view: GbaOverlayView, slot: Int, index: Int) {
        val sp = slotPrefs.getValue(slot)
        val sw =
            sp.getFloat(PREF_GBA_WIDTH, DEFAULT_GBA_WIDTH).coerceIn(GBA_MIN_WIDTH, GBA_MAX_WIDTH)
        val sh = sw / view.aspectRatio
        val metrics = activity.resources.displayMetrics
        val sx = sp.getFloat(PREF_GBA_X, DEFAULT_GBA_X + index * GBA_RESET_OFFSET)
            .coerceIn(0f, metrics.widthPixels.toFloat())
        val sy = sp.getFloat(
            PREF_GBA_Y,
            metrics.heightPixels - sh - DEFAULT_GBA_X - index * GBA_RESET_OFFSET
        ).coerceIn(0f, metrics.heightPixels.toFloat())
        view.setBounds(sw.toInt(), sh.toInt(), sx, sy)
        attachGbaTouchListener(view, slot, sp)
    }

    fun toggleGbaSnap() {
        isGbaLocked = !isGbaLocked
        if (!isGbaLocked) GbaRenderer.setTvLeftOffset(0)
        binding.root.post { applyGbaLayout() }
    }

    fun resetGbaScreens() {
        if (gbaViews.isEmpty() || isGbaLocked) return
        activity.runOnUiThread {
            val h = activity.resources.displayMetrics.heightPixels.toFloat()
            gbaViews.forEachIndexed { i, v ->
                val x = DEFAULT_GBA_X + i * GBA_RESET_OFFSET
                val y = h - DEFAULT_GBA_HEIGHT - DEFAULT_GBA_X - i * GBA_RESET_OFFSET
                slotPrefs.getValue(v.gbaSlot).edit {
                    putFloat(PREF_GBA_X, x)
                    putFloat(PREF_GBA_Y, y)
                    putFloat(PREF_GBA_WIDTH, DEFAULT_GBA_WIDTH)
                    putFloat(PREF_GBA_HEIGHT, DEFAULT_GBA_HEIGHT)
                }
                v.setBounds(DEFAULT_GBA_WIDTH.toInt(), DEFAULT_GBA_HEIGHT.toInt(), x, y)
            }
        }
    }

    fun resetGbaCore() = activeGbaSlots().forEach { GbaRenderer.resetGbaCore(it) }

    fun adjustGbaVolume() {
        val activeSlots = activeGbaSlots()
        if (activeSlots.isEmpty()) return

        val padding = (24 * activity.resources.displayMetrics.density).toInt()
        val content = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(padding, padding / 2, padding, 0)
        }

        activeSlots.forEach { slot ->
            val label = TextView(activity)
            val slider = Slider(activity).apply {
                valueFrom = 0f; valueTo = 100f; stepSize = 1f
                value = getGbaVolumePercent(slot).toFloat()
            }

            fun updateLabel(v: Int) {
                label.text = activity.getString(R.string.emulation_gba_volume_slot, slot + 1, v)
            }

            updateLabel(slider.value.toInt())
            slider.addOnChangeListener { _, value, _ ->
                value.toInt().also { updateLabel(it); setGbaVolumePercent(slot, it) }
            }

            content.addView(label)
            content.addView(slider)
        }

        MaterialAlertDialogBuilder(activity)
            .setTitle(R.string.emulation_gba_volume)
            .setView(content)
            .setNeutralButton(R.string.input_reset_to_default) { _, _ ->
                activeSlots.forEach { setGbaVolumePercent(it, 100) }
            }
            .setPositiveButton(R.string.ok, null)
            .show()
    }

    private fun applyStoredGbaVolume(slot: Int) = setGbaVolume(slot, getGbaVolumePercent(slot))

    private fun getGbaVolumePercent(slot: Int): Int =
        slotPrefs.getValue(slot).getInt(GBA_VOLUME_PREF, 100).coerceIn(0, 100)

    private fun setGbaVolumePercent(slot: Int, percent: Int) {
        val clampedPercent = percent.coerceIn(0, 100)
        slotPrefs.getValue(slot).edit { putInt(GBA_VOLUME_PREF, clampedPercent) }
        setGbaVolume(slot, clampedPercent)
    }

    private fun setGbaVolume(slot: Int, percent: Int) {
        GbaRenderer.setGbaVolume(slot, percent * MIXER_MAX_VOLUME / 100)
    }

    fun showGbaSlotSelection() {
        val activeSlots = activeGbaSlots()
        if (activeSlots.isEmpty()) {
            MaterialAlertDialogBuilder(activity)
                .setTitle(R.string.emulation_gba_slot_selection)
                .setPositiveButton(R.string.ok, null)
                .show()
            return
        }

        val currentValue = getGbaActiveSlot()
        val slotEntries =
            activeSlots.map { activity.getString(R.string.gba_slot_name, it + 1) }.toTypedArray()
        val checkedItem = activeSlots.indexOf(currentValue)

        MaterialAlertDialogBuilder(activity)
            .setTitle(R.string.emulation_gba_slot_selection)
            .setSingleChoiceItems(slotEntries, checkedItem) { dialog, indexSelected ->
                setGbaActiveSlot(activeSlots[indexSelected])
                dialog.dismiss()
                onRefreshOverlay?.invoke()
            }
            .setNeutralButton(R.string.emulation_more_controller_settings) { _, _ ->
                SettingsActivity.launch(activity, MenuTag.SETTINGS)
            }
            .show()
    }

    fun getGbaActiveSlot() = IntSetting.MAIN_GBA_ACTIVE_SLOT.int

    private fun setGbaActiveSlot(slot: Int) =
        settings?.let { IntSetting.MAIN_GBA_ACTIVE_SLOT.setInt(it, slot) }

    private companion object {
        const val DEFAULT_GBA_WIDTH = 480f
        const val DEFAULT_GBA_HEIGHT = 320f
        const val DEFAULT_GBA_X = 16f
        const val GBA_RESET_OFFSET = 20f
        const val PREF_GBA_OVERLAY_GLOBAL = "gba_overlay"
        const val PREF_GBA_LOCKED = "gba_locked"
        const val PREF_GBA_WIDTH = "gba_width"
        const val PREF_GBA_HEIGHT = "gba_height"
        const val PREF_GBA_X = "gba_x"
        const val PREF_GBA_Y = "gba_y"
        const val GBA_MIN_WIDTH = 120f
        const val GBA_MAX_WIDTH = 960f
        const val GBA_VOLUME_PREF = "gba_volume_percent"
        const val MIXER_MAX_VOLUME = 255
    }
}
