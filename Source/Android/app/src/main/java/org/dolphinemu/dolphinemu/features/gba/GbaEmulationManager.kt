// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.gba

import android.content.Context
import android.content.res.Configuration
import android.view.Gravity
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.View
import android.widget.FrameLayout
import kotlin.properties.Delegates
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.content.edit
import com.google.android.material.button.MaterialButton
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

    enum class GbaSnapMode(val value: Int) {
        NONE(0),
        SNAPPED(1)
    }

    private var snapMode by Delegates.observable(GbaSnapMode.NONE) { _, _, newValue ->
        globalGbaPrefs.edit { putInt(PREF_GBA_SNAP_MODE, newValue.value) }
        postApplyGbaLayout()
    }

    private var tvXPercent by Delegates.observable(0.5f) { _, _, newValue ->
        globalGbaPrefs.edit { putFloat(PREF_GBA_TV_X_PERCENT, newValue) }
        postApplyGbaLayout()
    }

    val isGbaSnapped: Boolean
        get() = snapMode != GbaSnapMode.NONE

    var isGbaTouchEnabled by Delegates.observable(true) { _, _, newValue ->
        globalGbaPrefs.edit { putBoolean(PREF_GBA_TOUCH_ENABLED, newValue) }
    }

    var isLinearFiltering by Delegates.observable(false) { _, _, newValue ->
        globalGbaPrefs.edit { putBoolean(PREF_GBA_LINEAR_FILTERING, newValue) }
        gbaViews.forEach { it.isLinearFiltering = newValue }
        if (GbaRenderer.isAttached()) {
            activeSlots.forEach { GbaRenderer.requestRedraw(it) }
        }
    }

    private var isMenuOpen = false
    private var activeTouchView: GbaOverlayView? = null
    private var isLayoutPending = false
    private var isBottomLayout = false

    private var settings: Settings? = null
    private var onRefreshOverlay: (() -> Unit)? = null

    private val gbaDefaultWidth by lazy { activity.resources.getDimension(R.dimen.gba_default_width) }
    private val gbaDefaultX by lazy { activity.resources.getDimension(R.dimen.gba_default_x) }
    private val gbaResetOffset by lazy { activity.resources.getDimension(R.dimen.gba_reset_offset) }
    private val gbaMinWidth by lazy { activity.resources.getDimension(R.dimen.gba_min_width) }
    private val gbaMaxWidth by lazy { activity.resources.getDimension(R.dimen.gba_max_width) }

    private val slotPrefs = (0 until 4).associateWith { slot ->
        activity.getSharedPreferences("$PREF_GBA_OVERLAY_GLOBAL$slot", Context.MODE_PRIVATE)
    }
    private val globalGbaPrefs =
        activity.getSharedPreferences(PREF_GBA_OVERLAY_GLOBAL, Context.MODE_PRIVATE)

    fun initSettings(settings: Settings, onRefreshOverlay: () -> Unit) {
        this.settings = settings
        this.onRefreshOverlay = onRefreshOverlay
    }

    private val activeSlots: List<Int>
        get() = (0..3).filter {
            IntSetting.getSettingForSIDevice(it).int == InputOverlay.EMULATED_GBA_CONTROLLER
        }

    fun initViews() {
        val legacySnapped = globalGbaPrefs.getBoolean("gba_locked", false)
        snapMode = gbaSnapModeFromInt(
            globalGbaPrefs.getInt(
                PREF_GBA_SNAP_MODE,
                if (legacySnapped) GbaSnapMode.SNAPPED.value else GbaSnapMode.NONE.value
            )
        )
        tvXPercent = globalGbaPrefs.getFloat(PREF_GBA_TV_X_PERCENT, 0.5f)
        isGbaTouchEnabled = globalGbaPrefs.getBoolean(PREF_GBA_TOUCH_ENABLED, true)
        isLinearFiltering = globalGbaPrefs.getBoolean(PREF_GBA_LINEAR_FILTERING, false)

        activity.isMenuShowing.observe(activity) { open ->
            if (open != isMenuOpen) {
                isMenuOpen = open
                if (!open && !isGbaSnapped) reattachTouchListeners()
            }
        }

        syncGbaViews()

        ControllerInterface.devicesChanged.observe(activity) {
            updateGbaDimmingState()
        }
    }

    fun onTitleChanged() = syncGbaViews()

    private fun syncGbaViews() {
        ensureValidGbaSlot()
        val slots = activeSlots

        while (gbaViews.size < slots.size) {
            val slot = slots[gbaViews.size]
            val view = GbaOverlayView(activity).apply {
                gbaSlot = slot
                isLinearFiltering = this@GbaEmulationManager.isLinearFiltering
                onDimensionsChanged = { applyGbaLayout() }
            }
            binding.root.addView(view, 0)
            gbaViews.add(view)
            applyStoredGbaVolume(slot)
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
            if (NativeLibrary.IsGameMetadataValid()) {
                if (GbaRenderer.isAttached()) GbaRenderer.updateViews(gbaViews)
                else GbaRenderer.attach(gbaViews)
            }
            binding.root.post { applyGbaLayout() }
        }
    }

    fun onDestroy() {
        GbaRenderer.detach()
        GbaRenderer.setTvLeftOffset(0)
        GbaRenderer.setTvRightOffset(0)
        GbaRenderer.setTvTopOffset(0)
        GbaRenderer.setTvBottomOffset(0)
        gbaViews.forEach { binding.root.removeView(it) }
        gbaViews.clear()
    }

    fun onConfigurationChanged() {
        if (gbaViews.isNotEmpty()) {
            GbaRenderer.updateViews(gbaViews)
            binding.root.post { applyGbaLayout() }
        }
    }

    private var lastDragX = 0f
    private var isDraggingTv = false
    private var isEditingSnapLayout = false
    private var editButtonsContainer: View? = null

    fun handleTouch(event: MotionEvent): Boolean {
        if (isMenuOpen || gbaViews.isEmpty()) return false

        val action = event.actionMasked
        val tx = event.x
        val ty = event.y

        if (isEditingSnapLayout) {
            editButtonsContainer?.let { container ->
                val loc = IntArray(2)
                container.getLocationInWindow(loc)
                if (tx >= loc[0] && tx <= loc[0] + container.width &&
                    ty >= loc[1] && ty <= loc[1] + container.height
                ) {
                    return false
                }
            }
        }

        // 3-finger switch even if locked
        if (action == MotionEvent.ACTION_POINTER_DOWN && event.pointerCount == 3) {
            findViewAt(event.rawX.toInt(), event.rawY.toInt())?.let { view ->
                selectGbaSlot(view.gbaSlot)
                activeTouchView = null
                return true
            }
        }

        val isLandscape =
            activity.resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

        if (isGbaSnapped && isLandscape && isEditingSnapLayout && !isBottomLayout) {
            when (action) {
                MotionEvent.ACTION_DOWN -> {
                    val rootWidth = binding.root.width
                    val tvWidth = GbaRenderer.getTvDrawWidth()
                    val blackBarTotalX = (rootWidth - tvWidth).coerceAtLeast(0)

                    val tvLeft = blackBarTotalX * tvXPercent

                    if (tx >= tvLeft && tx <= tvLeft + tvWidth) {
                        isDraggingTv = true
                        lastDragX = event.rawX
                        return true
                    }
                }

                MotionEvent.ACTION_MOVE -> {
                    if (isDraggingTv) {
                        val deltaX = event.rawX - lastDragX
                        lastDragX = event.rawX

                        val rootWidth = binding.root.width
                        val tvWidth = GbaRenderer.getTvDrawWidth()

                        val totalBlackBarX = (rootWidth - tvWidth).coerceAtLeast(1)

                        tvXPercent = (tvXPercent + deltaX / totalBlackBarX).coerceIn(0f, 1f)
                        return true
                    }
                }

                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    isDraggingTv = false
                }
            }
        }

        if (isGbaSnapped || !isGbaTouchEnabled) return false

        if (action == MotionEvent.ACTION_DOWN) {
            activeTouchView = findViewAt(event.rawX.toInt(), event.rawY.toInt())
        }

        val handled = activeTouchView?.dispatchTouchEvent(event) ?: false
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) activeTouchView =
            null

        return handled
    }

    private fun findViewAt(x: Int, y: Int): GbaOverlayView? {
        val loc = IntArray(2)
        return gbaViews.find { view ->
            if (view.visibility != View.VISIBLE) return@find false
            view.getLocationOnScreen(loc)
            tempRect.set(loc[0], loc[1], loc[0] + view.width, loc[1] + view.height)
            tempRect.contains(x, y)
        }
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
        if (isGbaSnapped) return
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
                    if (isGbaSnapped) return true
                    val ratio = view.aspectRatio
                    val ow = view.width.toFloat()
                    val oh = view.height.toFloat()
                    val nw = (ow * d.scaleFactor).coerceIn(gbaMinWidth, gbaMaxWidth)
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
            if (isGbaSnapped) return@setOnTouchListener false

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
        isLayoutPending = false
        if (gbaViews.isEmpty()) return

        // Wait for at least one frame to be presented so we have valid frame dimensions when snapped.
        if (isGbaSnapped && GbaRenderer.getFrameCount() < 1) {
            binding.root.postDelayed({
                applyGbaLayout()
            }, 100)
            return
        }

        val root = binding.root
        if (root.width <= 0 || root.height <= 0) {
            postApplyGbaLayout()
            return
        }
        val rootWidth = root.width
        val rootHeight = root.height

        val isLandscape =
            activity.resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

        when {
            isGbaSnapped && isLandscape -> applySnappedLandscapeLayout(rootWidth, rootHeight)
            isGbaSnapped -> applySnappedPortraitLayout(rootWidth, rootHeight)
            else -> restoreUnlockedLayout()
        }

        activeSlots.forEach { applyStoredGbaVolume(it) }
        updateGbaDimmingState()
    }

    private fun applySnappedLandscapeLayout(rootWidth: Int, rootHeight: Int) {
        if (gbaViews.isEmpty()) return

        // Calculate TV width based on game aspect ratio to avoid shrinking feedback loops.
        // The main TV view is the priority and should only move within available black bars if possible.
        val gameRatio = NativeLibrary.GetGameAspectRatio().takeIf { it > 0 } ?: 1.333f
        val naturalTvWidth = (rootHeight * gameRatio).coerceAtMost(rootWidth.toFloat()).toInt()
        val blackBarTotalX = (rootWidth - naturalTvWidth).coerceAtLeast(0)

        if (blackBarTotalX < rootWidth * 0.14f) {
            isBottomLayout = true
            applySnappedLandscapeBottomLayout(rootWidth, rootHeight)
            return
        }

        isBottomLayout = false
        GbaRenderer.setTvTopOffset(0)
        GbaRenderer.setTvBottomOffset(0)

        // Snap to center/edges if close
        val effectiveX = when {
            tvXPercent < 0.05f -> 0f
            tvXPercent > 0.95f -> 1f
            tvXPercent in 0.45f..0.55f -> 0.5f
            else -> tvXPercent
        }

        val leftOffset = (blackBarTotalX * effectiveX).toInt()
        val rightOffset = blackBarTotalX - leftOffset

        val leftCount = Math.round(gbaViews.size * effectiveX).coerceIn(0, gbaViews.size)
        val rightCount = gbaViews.size - leftCount

        val leftViews = gbaViews.take(leftCount)
        val rightViews = gbaViews.takeLast(rightCount)

        val leftSlotHeight = if (leftCount > 0) rootHeight / leftCount else rootHeight
        val rightSlotHeight = if (rightCount > 0) rootHeight / rightCount else rootHeight

        leftViews.forEachIndexed { i, view ->
            view.isScreenVisible = true; view.needsBorderRedraw = false
            val ratio = view.aspectRatio
            var tw = leftOffset
            var th = (tw / ratio).toInt()
            if (th > leftSlotHeight) {
                th = leftSlotHeight; tw = (th * ratio).toInt()
            }
            view.setBounds(tw, th, 0f, i * leftSlotHeight + (leftSlotHeight - th) / 2f)
        }

        rightViews.forEachIndexed { i, view ->
            view.isScreenVisible = true; view.needsBorderRedraw = false
            val ratio = view.aspectRatio
            var tw = rightOffset
            var th = (tw / ratio).toInt()
            if (th > rightSlotHeight) {
                th = rightSlotHeight; tw = (th * ratio).toInt()
            }
            view.setBounds(
                tw,
                th,
                (rootWidth - tw).toFloat(),
                i * rightSlotHeight + (rightSlotHeight - th) / 2f
            )
        }

        GbaRenderer.setTvLeftOffset(leftOffset)
        GbaRenderer.setTvRightOffset(rightOffset)
    }

    private fun applySnappedLandscapeBottomLayout(rootWidth: Int, rootHeight: Int) {
        GbaRenderer.setTvLeftOffset(0)
        GbaRenderer.setTvRightOffset(0)

        val count = gbaViews.size
        val maxGbaWidth = rootWidth / count
        val ratio = gbaViews.firstOrNull()?.aspectRatio ?: 1.5f
        val maxGbaHeight = (rootHeight * 0.3f).toInt()

        var tw = maxGbaWidth
        var th = (tw / ratio).toInt()
        if (th > maxGbaHeight) {
            th = maxGbaHeight
            tw = (th * ratio).toInt()
        }

        val bottomSpace = th
        GbaRenderer.setTvTopOffset(0)
        GbaRenderer.setTvBottomOffset(bottomSpace)

        val totalRowWidth = tw * count
        val startX = (rootWidth - totalRowWidth) / 2f

        gbaViews.forEachIndexed { i, view ->
            view.isScreenVisible = true
            view.needsBorderRedraw = false
            view.setBounds(
                tw,
                th,
                startX + i * tw,
                (rootHeight - th).toFloat()
            )
        }
    }

    private fun applySnappedPortraitLayout(rootWidth: Int, rootHeight: Int) {
        isBottomLayout = false
        GbaRenderer.setTvLeftOffset(0)
        GbaRenderer.setTvRightOffset(0)
        GbaRenderer.setTvTopOffset(0)
        GbaRenderer.setTvBottomOffset(0)
        val gbaTop = GbaRenderer.getTvDrawTop() + GbaRenderer.getTvDrawHeight()
        val cols = if (gbaViews.size <= 2) gbaViews.size else 2
        val sw = rootWidth / cols

        gbaViews.forEachIndexed { i, view ->
            val ratio = view.aspectRatio
            val th = (sw / ratio).toInt()
                .coerceAtMost((rootHeight - gbaTop) / ((gbaViews.size + cols - 1) / cols))
                .coerceAtMost(400)
            val tw = (th * ratio).toInt()
            view.setBounds(
                tw,
                th,
                (i % cols) * sw + (sw - tw) / 2f,
                (gbaTop + (i / cols) * th).toFloat()
            )
        }
    }

    private fun restoreUnlockedLayout() {
        isBottomLayout = false
        GbaRenderer.setTvLeftOffset(0)
        GbaRenderer.setTvRightOffset(0)
        GbaRenderer.setTvTopOffset(0)
        GbaRenderer.setTvBottomOffset(0)
        gbaViews.forEachIndexed { index, view ->
            restoreViewFromPrefs(view, view.gbaSlot, index)
        }
    }

    private fun postApplyGbaLayout() {
        if (isLayoutPending) return
        isLayoutPending = true
        binding.root.post {
            applyGbaLayout()
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
            sp.getFloat(PREF_GBA_WIDTH, gbaDefaultWidth).coerceIn(gbaMinWidth, gbaMaxWidth)
        val sh = sw / view.aspectRatio
        val metrics = activity.resources.displayMetrics
        val sx = sp.getFloat(PREF_GBA_X, gbaDefaultX + index * gbaResetOffset)
            .coerceIn(0f, metrics.widthPixels.toFloat())
        val sy = sp.getFloat(
            PREF_GBA_Y,
            metrics.heightPixels - sh - gbaDefaultX - index * gbaResetOffset
        ).coerceIn(0f, metrics.heightPixels.toFloat())
        view.setBounds(sw.toInt(), sh.toInt(), sx, sy)
        attachGbaTouchListener(view, slot, sp)
    }

    fun toggleGbaSnap() {
        snapMode = GbaSnapMode.SNAPPED
        isEditingSnapLayout = true
        showEditButtons()
    }

    private fun showEditButtons() {
        activity.runOnUiThread {
            if (editButtonsContainer != null) return@runOnUiThread
            val container = LinearLayout(activity).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER
            }

            val doneBtn = MaterialButton(activity).apply {
                text = "Done"
                setOnClickListener {
                    isEditingSnapLayout = false
                    isDraggingTv = false
                    hideEditButtons()
                    binding.root.postInvalidate()
                }
            }

            val unsnapBtn = MaterialButton(activity).apply {
                text = "Unsnap"
                setOnClickListener {
                    snapMode = GbaSnapMode.NONE
                    isEditingSnapLayout = false
                    isDraggingTv = false
                    hideEditButtons()
                    GbaRenderer.setTvLeftOffset(0)
                    GbaRenderer.setTvRightOffset(0)
                    GbaRenderer.setTvTopOffset(0)
                    GbaRenderer.setTvBottomOffset(0)
                    applyGbaLayout()
                }
            }

            val params = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                setMargins(20, 0, 20, 0)
            }

            container.addView(unsnapBtn, params)
            container.addView(doneBtn, params)

            val frameParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                gravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
                bottomMargin = 100
            }

            binding.root.addView(container, frameParams)
            container.bringToFront()
            editButtonsContainer = container
        }
    }

    private fun hideEditButtons() {
        activity.runOnUiThread {
            editButtonsContainer?.let {
                binding.root.removeView(it)
                editButtonsContainer = null
            }
        }
    }

    fun toggleGbaTouch() {
        isGbaTouchEnabled = !isGbaTouchEnabled
    }

    fun toggleLinearFiltering() {
        isLinearFiltering = !isLinearFiltering
    }

    fun resetGbaScreens() {
        if (gbaViews.isEmpty() || isGbaSnapped) return
        activity.runOnUiThread {
            val h = activity.resources.displayMetrics.heightPixels.toFloat()
            gbaViews.forEachIndexed { i, v ->
                val x = gbaDefaultX + i * gbaResetOffset
                val y = h - v.height - gbaDefaultX - i * gbaResetOffset
                slotPrefs[v.gbaSlot]?.edit {
                    putFloat(PREF_GBA_X, x); putFloat(PREF_GBA_Y, y)
                }
                v.x = x
                v.y = y
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
        val options =
            arrayOf(R.string.emulation_reset_gba_game, R.string.emulation_reset_gba_multiboot)
                .map { activity.getString(it) }.toTypedArray()

        MaterialAlertDialogBuilder(activity)
            .setTitle(activity.getString(R.string.emulation_reset_gba_title, slot + 1))
            .setItems(options) { _, which -> confirmReset(slot, isMultiboot = (which == 1)) }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    private fun confirmReset(slot: Int, isMultiboot: Boolean) {
        val msgId =
            if (isMultiboot) R.string.emulation_reset_gba_multiboot_confirm else R.string.emulation_reset_gba_confirm

        MaterialAlertDialogBuilder(activity)
            .setTitle(R.string.emulation_reset_gba)
            .setMessage(activity.getString(msgId, slot + 1))
            .setPositiveButton(R.string.ok) { _, _ ->
                if (isMultiboot) {
                    val romSetting = listOf(
                        StringSetting.MAIN_GBA_ROM_PATH_1, StringSetting.MAIN_GBA_ROM_PATH_2,
                        StringSetting.MAIN_GBA_ROM_PATH_3, StringSetting.MAIN_GBA_ROM_PATH_4
                    ).getOrNull(slot)
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
        val slots = activeSlots.takeIf { it.isNotEmpty() } ?: return
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
                addOnChangeListener { _, v, _ ->
                    v.toInt().let {
                        label.text =
                            activity.getString(R.string.emulation_gba_volume_slot, slot + 1, it)
                        setGbaVolumePercent(slot, it)
                    }
                }
            }
            label.text = activity.getString(
                R.string.emulation_gba_volume_slot,
                slot + 1,
                slider.value.toInt()
            )
            content.apply { addView(label); addView(slider) }
        }

        MaterialAlertDialogBuilder(activity)
            .setTitle(R.string.emulation_gba_volume)
            .setView(content)
            .setNeutralButton(R.string.input_reset_to_default) { _, _ ->
                slots.forEach {
                    setGbaVolumePercent(
                        it,
                        100
                    )
                }
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
        val controllerIdx =
            (if (isWii) IntSetting.MAIN_OVERLAY_WII_CONTROLLER else IntSetting.MAIN_OVERLAY_GC_CONTROLLER).int
        val selected = IntSetting.MAIN_GBA_ACTIVE_SLOT.int

        return when {
            controllerIdx in slots -> controllerIdx
            selected in slots -> selected
            else -> slots.firstOrNull() ?: selected
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
        fun gbaSnapModeFromInt(value: Int) =
            GbaSnapMode.entries.firstOrNull { it.value == value } ?: GbaSnapMode.NONE

        const val PREF_GBA_OVERLAY_GLOBAL = "gba_overlay"
        const val PREF_GBA_SNAP_MODE = "gba_snap_mode"
        const val PREF_GBA_TV_X_PERCENT = "gba_tv_x_percent"
        const val PREF_GBA_TOUCH_ENABLED = "gba_touch_enabled"
        const val PREF_GBA_LINEAR_FILTERING = "gba_linear_filtering"
        const val PREF_GBA_WIDTH = "gba_width"
        const val PREF_GBA_HEIGHT = "gba_height"
        const val PREF_GBA_X = "gba_x"
        const val PREF_GBA_Y = "gba_y"
        const val GBA_VOLUME_PREF = "gba_volume_percent"
        const val MIXER_MAX_VOLUME = 255
    }
}
