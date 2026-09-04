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
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.features.settings.model.StringSetting
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
    private val lastGbaTapTimes = mutableMapOf<Int, Long>()
    private val tempRect = android.graphics.Rect()
    var isGbaLocked = false
    var isTouchPassthrough = false
    private var isMenuOpen = false
    private var isWaitingForGcOffsetReset = false
    private var activeTouchView: GbaOverlayView? = null

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

    private val activeSlots: List<Int>
        get() = (0..3).filter {
            IntSetting.getSettingForSIDevice(it).int == InputOverlay.EMULATED_GBA_CONTROLLER
        }

    fun initViews() {
        isGbaLocked = globalGbaPrefs.getBoolean(PREF_GBA_LOCKED, false)
        isTouchPassthrough = globalGbaPrefs.getBoolean(PREF_GBA_TOUCH_PASSTHROUGH, false)

        activity.isMenuShowing.observe(activity) { open ->
            if (open != isMenuOpen) {
                isMenuOpen = open
                if (!open && !isGbaLocked) reattachTouchListeners()
            }
        }

        activeSlots.forEach { slot ->
            val view = GbaOverlayView(activity).apply {
                gbaSlot = slot
                onDimensionsChanged = { applyGbaLayout() }
            }
            binding.root.addView(view, 0)
            gbaViews.add(view)
            applyStoredGbaVolume(slot)
            restoreViewFromPrefs(view, slot, gbaViews.size - 1)
        }

        ensureValidGbaSlot()
        updateGbaDimmingState()

        ControllerInterface.devicesChanged.observe(activity) {
            updateGbaDimmingState()
        }

        if (gbaViews.isNotEmpty() && NativeLibrary.IsGameMetadataValid()) {
            GbaRenderer.attach(gbaViews)
        }
    }

    fun onTitleChanged() {
        ensureValidGbaSlot()
        val slots = activeSlots

        while (gbaViews.size < slots.size) {
            val slot = slots[gbaViews.size]
            val view = GbaOverlayView(activity).apply {
                gbaSlot = slot
                onDimensionsChanged = { applyGbaLayout() }
            }
            binding.root.addView(view, 0)
            gbaViews.add(view)
            restoreViewFromPrefs(view, slot, gbaViews.size - 1)
        }

        gbaViews.forEachIndexed { i, v ->
            if (i < slots.size) {
                v.gbaSlot = slots[i]
                v.visibility = View.VISIBLE
            } else {
                v.visibility = View.GONE
            }
        }
        updateGbaDimmingState()
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
        if (isMenuOpen || gbaViews.isEmpty()) return false

        val action = event.actionMasked

        // 3-finger switch even if locked
        if (action == MotionEvent.ACTION_POINTER_DOWN && event.pointerCount == 3) {
            val x = event.rawX.toInt()
            val y = event.rawY.toInt()
            val loc = IntArray(2)
            gbaViews.find { view ->
                view.getLocationOnScreen(loc)
                tempRect.set(loc[0], loc[1], loc[0] + view.width, loc[1] + view.height)
                tempRect.contains(x, y)
            }?.let { view ->
                selectGbaSlot(view.gbaSlot)
                activeTouchView = null
                return true
            }
        }

        if (isGbaLocked || isTouchPassthrough) return false

        if (action == MotionEvent.ACTION_DOWN) {
            val x = event.rawX.toInt()
            val y = event.rawY.toInt()
            val loc = IntArray(2)
            activeTouchView = gbaViews.find { view ->
                view.getLocationOnScreen(loc)
                tempRect.set(loc[0], loc[1], loc[0] + view.width, loc[1] + view.height)
                tempRect.contains(x, y)
            }
        }

        val handled = activeTouchView?.dispatchTouchEvent(event) ?: false

        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            activeTouchView = null
        }

        return handled
    }

    fun setGbaViewsTouchable(touchable: Boolean) {
        gbaViews.forEach { view ->
            view.isClickable = touchable
            view.isFocusable = touchable
            view.isFocusableInTouchMode = touchable
            if (!touchable) {
                view.setOnTouchListener(null)
            }
        }
        if (touchable) {
            reattachTouchListeners()
        }
    }

    fun reattachTouchListeners() {
        if (isGbaLocked) return
        gbaViews.forEach { view ->
            slotPrefs[view.gbaSlot]?.let { attachGbaTouchListener(view, view.gbaSlot, it) }
        }
    }

    private fun attachGbaTouchListener(
        view: GbaOverlayView,
        slot: Int,
        sp: android.content.SharedPreferences
    ) {
        var dragX = 0f
        var dragY = 0f
        var needsDragSync = false
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
                    val dx = (ow - nw) / 2f
                    val dy = (oh - nh) / 2f
                    view.x += dx; view.y += dy
                    dragX -= dx; dragY -= dy
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

            val wasScaling = scaleDetector.isInProgress
            scaleDetector.onTouchEvent(event)
            if (scaleDetector.isInProgress) return@setOnTouchListener true
            if (wasScaling) needsDragSync = true

            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    dragX = event.rawX - v.x; dragY = event.rawY - v.y
                    needsDragSync = false
                }

                MotionEvent.ACTION_POINTER_DOWN, MotionEvent.ACTION_POINTER_UP -> {
                    needsDragSync = true
                }

                MotionEvent.ACTION_MOVE -> {
                    if (needsDragSync) {
                        dragX = event.rawX - v.x; dragY = event.rawY - v.y
                        needsDragSync = false
                    }
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

        globalGbaPrefs.edit { putBoolean(PREF_GBA_LOCKED, isGbaLocked) }
        activeSlots.forEach { applyStoredGbaVolume(it) }
        updateGbaDimmingState()
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
            val x = col * sw + (sw - tw) / 2f
            val y = (gbaTop + row * th).toFloat()

            view.setBounds(tw, th, x, y)
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
        val sp = slotPrefs[slot] ?: return
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

    fun toggleTouchPassthrough() {
        isTouchPassthrough = !isTouchPassthrough
        globalGbaPrefs.edit { putBoolean(PREF_GBA_TOUCH_PASSTHROUGH, isTouchPassthrough) }
    }

    fun resetGbaScreens() {
        if (gbaViews.isEmpty() || isGbaLocked) return
        activity.runOnUiThread {
            val h = activity.resources.displayMetrics.heightPixels.toFloat()
            gbaViews.forEachIndexed { i, v ->
                val x = DEFAULT_GBA_X + i * GBA_RESET_OFFSET
                val y = h - DEFAULT_GBA_HEIGHT - DEFAULT_GBA_X - i * GBA_RESET_OFFSET
                slotPrefs[v.gbaSlot]?.edit {
                    putFloat(PREF_GBA_X, x)
                    putFloat(PREF_GBA_Y, y)
                    putFloat(PREF_GBA_WIDTH, DEFAULT_GBA_WIDTH)
                    putFloat(PREF_GBA_HEIGHT, DEFAULT_GBA_HEIGHT)
                }
                v.setBounds(DEFAULT_GBA_WIDTH.toInt(), DEFAULT_GBA_HEIGHT.toInt(), x, y)
            }
        }
    }

    fun resetGbaCore() {
        val slots = activeSlots
        if (slots.isEmpty()) return

        if (slots.size == 1) {
            showResetOptions(slots[0])
        } else {
            val slotEntries =
                slots.map { activity.getString(R.string.gba_slot_name, it + 1) }.toTypedArray()
            MaterialAlertDialogBuilder(activity)
                .setTitle(R.string.emulation_reset_gba)
                .setItems(slotEntries) { _, index ->
                    showResetOptions(slots[index])
                }
                .setNegativeButton(R.string.cancel, null)
                .show()
        }
    }

    private fun showResetOptions(slot: Int) {
        val options = arrayOf(
            activity.getString(R.string.emulation_reset_gba_game),
            activity.getString(R.string.emulation_reset_gba_multiboot)
        )

        MaterialAlertDialogBuilder(activity)
            .setTitle(activity.getString(R.string.emulation_reset_gba_title, slot + 1))
            .setItems(options) { _, which ->
                when (which) {
                    0 -> confirmReset(slot, isMultiboot = false)
                    1 -> confirmReset(slot, isMultiboot = true)
                }
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    private fun confirmReset(slot: Int, isMultiboot: Boolean) {
        val message = if (isMultiboot) {
            activity.getString(R.string.emulation_reset_gba_multiboot_confirm, slot + 1)
        } else {
            activity.getString(R.string.emulation_reset_gba_confirm, slot + 1)
        }

        MaterialAlertDialogBuilder(activity)
            .setTitle(R.string.emulation_reset_gba)
            .setMessage(message)
            .setPositiveButton(R.string.ok) { _, _ ->
                if (isMultiboot) {
                    val romSetting = when (slot) {
                        0 -> StringSetting.MAIN_GBA_ROM_PATH_1
                        1 -> StringSetting.MAIN_GBA_ROM_PATH_2
                        2 -> StringSetting.MAIN_GBA_ROM_PATH_3
                        3 -> StringSetting.MAIN_GBA_ROM_PATH_4
                        else -> null
                    }
                    settings?.let { romSetting?.setString(it, "") }
                    GbaRenderer.resetToMultiboot(slot)
                } else {
                    GbaRenderer.resetGbaCore(slot)
                }
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    fun adjustGbaVolume() {
        val slots = activeSlots
        if (slots.isEmpty()) return

        val padding = (24 * activity.resources.displayMetrics.density).toInt()
        val content = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(padding, padding / 2, padding, 0)
        }

        slots.forEach { slot ->
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
                slots.forEach { setGbaVolumePercent(it, 100) }
            }
            .setPositiveButton(R.string.ok, null)
            .show()
    }

    private fun applyStoredGbaVolume(slot: Int) = setGbaVolume(slot, getGbaVolumePercent(slot))

    private fun getGbaVolumePercent(slot: Int): Int =
        slotPrefs[slot]?.getInt(GBA_VOLUME_PREF, 100)?.coerceIn(0, 100) ?: 100

    private fun setGbaVolumePercent(slot: Int, percent: Int) {
        val clampedPercent = percent.coerceIn(0, 100)
        slotPrefs[slot]?.edit { putInt(GBA_VOLUME_PREF, clampedPercent) }
        setGbaVolume(slot, clampedPercent)
    }

    private fun setGbaVolume(slot: Int, percent: Int) {
        GbaRenderer.setGbaVolume(slot, percent * MIXER_MAX_VOLUME / 100)
    }

    fun showGbaSlotSelection() {
        val slots = activeSlots
        if (slots.isEmpty()) {
            MaterialAlertDialogBuilder(activity)
                .setTitle(R.string.emulation_gba_slot_selection)
                .setPositiveButton(R.string.ok, null)
                .show()
            return
        }

        val currentValue = getGbaActiveSlot()
        val slotEntries =
            slots.map { activity.getString(R.string.gba_slot_name, it + 1) }.toTypedArray()
        val checkedItem = slots.indexOf(currentValue)

        MaterialAlertDialogBuilder(activity)
            .setTitle(R.string.emulation_gba_slot_selection)
            .setSingleChoiceItems(slotEntries, checkedItem) { dialog, indexSelected ->
                selectGbaSlot(slots[indexSelected])
                dialog.dismiss()
            }
            .setNeutralButton(R.string.emulation_more_controller_settings) { _, _ ->
                SettingsActivity.launch(activity, MenuTag.SETTINGS)
            }
            .show()
    }

    private fun selectGbaSlot(slot: Int) {
        setGbaActiveSlot(slot)

        val isWii = NativeLibrary.IsGameMetadataValid() && NativeLibrary.IsEmulatingWii()
        val controllerSetting =
            if (isWii) IntSetting.MAIN_OVERLAY_WII_CONTROLLER else IntSetting.MAIN_OVERLAY_GC_CONTROLLER

        if (controllerSetting.int in 0..3 &&
            IntSetting.getSettingForSIDevice(controllerSetting.int).int == InputOverlay.EMULATED_GBA_CONTROLLER
        ) {
            settings?.let { controllerSetting.setInt(it, slot) }
        }

        updateGbaDimmingState()
        onRefreshOverlay?.invoke()
    }

    fun getGbaActiveSlot(): Int {
        val slots = activeSlots
        val isWii = NativeLibrary.IsGameMetadataValid() && NativeLibrary.IsEmulatingWii()
        val controllerIndex =
            (if (isWii) IntSetting.MAIN_OVERLAY_WII_CONTROLLER else IntSetting.MAIN_OVERLAY_GC_CONTROLLER).int
        val selectedSlot = IntSetting.MAIN_GBA_ACTIVE_SLOT.int

        return when {
            controllerIndex in slots -> controllerIndex
            selectedSlot in slots -> selectedSlot
            else -> slots.firstOrNull() ?: selectedSlot
        }
    }

    private fun ensureValidGbaSlot() {
        val current = IntSetting.MAIN_GBA_ACTIVE_SLOT.int
        val slots = activeSlots
        if (current !in slots) {
            slots.firstOrNull()?.let { setGbaActiveSlot(it) }
        }
    }

    fun updateGbaDimmingState() {
        val activeOverlaySlot = getGbaActiveSlot()
        gbaViews.forEach { view ->
            val slot = view.gbaSlot
            view.isDimmed = slot != activeOverlaySlot && !hasPhysicalController(slot)
        }
        if (GbaRenderer.isAttached()) {
            GbaRenderer.updateViews(gbaViews)
        }
    }

    private fun hasPhysicalController(slot: Int): Boolean {
        val pad = EmulatedController.getGbaPad(slot)
        val device = pad.getDefaultDevice()
        return device.isNotEmpty() && !device.contains("Dolphin Touch", ignoreCase = true)
    }

    private fun setGbaActiveSlot(slot: Int) =
        settings?.let { IntSetting.MAIN_GBA_ACTIVE_SLOT.setInt(it, slot) }

    private companion object {
        const val DEFAULT_GBA_WIDTH = 480f
        const val DEFAULT_GBA_HEIGHT = 320f
        const val DEFAULT_GBA_X = 16f
        const val GBA_RESET_OFFSET = 20f
        const val PREF_GBA_OVERLAY_GLOBAL = "gba_overlay"
        const val PREF_GBA_LOCKED = "gba_locked"
        const val PREF_GBA_TOUCH_PASSTHROUGH = "gba_touch_passthrough"
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
