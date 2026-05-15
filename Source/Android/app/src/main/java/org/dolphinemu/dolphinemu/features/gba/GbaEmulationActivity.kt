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
import androidx.core.view.doOnLayout
import androidx.fragment.app.FragmentManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.ActivityEmulationBinding
import org.dolphinemu.dolphinemu.features.input.model.InputOverrider
import org.dolphinemu.dolphinemu.features.settings.model.IntSetting
import org.dolphinemu.dolphinemu.overlay.InputOverlay
import org.dolphinemu.dolphinemu.R

class GbaEmulationActivity(
    private val activity: EmulationActivity,
    private val binding: ActivityEmulationBinding
) {
    val gbaViews = mutableListOf<GbaOverlayView>()
    val lastGbaTapTimes = mutableMapOf<Int, Long>()
    var isGbaLocked = false
    private var isMenuOpen = false
    private var lockedLandscapeLayoutGeneration = 0
    private var isWaitingForGcOffsetReset = false

    // Property to handle aspect ratio calculations consistently.
    private val GbaOverlayView.aspectRatio: Float
        get() = if (nativeWidth > 0 && nativeHeight > 0) {
            nativeWidth.toFloat() / nativeHeight.toFloat()
        } else {
            1.5f
        }

// Check if menu is open so Gba screens don't conflict with touch.
    private val backStackListener = FragmentManager.OnBackStackChangedListener {
        val fm = activity.supportFragmentManager
        val menuOpen =
            fm.findFragmentById(R.id.frame_menu) != null || fm.findFragmentById(R.id.frame_submenu) != null
        if (menuOpen == isMenuOpen) return@OnBackStackChangedListener
        isMenuOpen = menuOpen
        if (!isMenuOpen && !isGbaLocked) reattachTouchListeners()
    }

    private fun slotPrefs(slot: Int) =
        activity.getSharedPreferences("gba_overlay_${slot}", Context.MODE_PRIVATE)

    private fun activeGbaSlots() = (0 until 4).filter {
        IntSetting.getSettingForSIDevice(it).int == InputOverlay.EMULATED_GBA_CONTROLLER
    }

    fun initViews() {
        val globalGbaPrefs = activity.getSharedPreferences("gba_overlay", Context.MODE_PRIVATE)
        isGbaLocked = globalGbaPrefs.getBoolean("gba_locked", false)
        activity.supportFragmentManager.addOnBackStackChangedListener(backStackListener)

        for (slot in activeGbaSlots()) {
            val view = GbaOverlayView(activity).apply {
                gbaSlot = slot
                onDimensionsChanged = { requestGbaLayout() }
                visibility = View.VISIBLE
            }

            val sp = slotPrefs(slot)
            val sw = sp.getFloat("gba_width", DEFAULT_GBA_WIDTH).coerceIn(120f, 960f)
            val sh = sw / view.aspectRatio

            binding.root.addView(view, 0, FrameLayout.LayoutParams(sw.toInt(), sh.toInt()))

            InputOverrider.registerGba(slot)
            applyStoredGbaVolume(slot)
            attachGbaTouchListener(view, slot, sp)
            gbaViews.add(view)
        }

        if (gbaViews.isNotEmpty() && NativeLibrary.IsGameMetadataValid()) {
            GbaRenderManager.attach(gbaViews)
            binding.root.doOnLayout { requestGbaLayout(retryLockedLandscape = true) }
        }
    }

    fun onTitleChanged() {
        val activeSlots = activeGbaSlots()
        gbaViews.forEachIndexed { index, view ->
            if (index < activeSlots.size) {
                view.gbaSlot = activeSlots[index]
                view.visibility = View.VISIBLE
                InputOverrider.registerGba(activeSlots[index])
            } else {
                view.visibility = View.GONE
            }
        }
        if (gbaViews.isNotEmpty()) {
            if (GbaRenderManager.isAttached()) {
                GbaRenderManager.updateViews(gbaViews)
            } else {
                GbaRenderManager.attach(gbaViews)
            }
            requestGbaLayout(retryLockedLandscape = true)
        }
    }

    fun onDestroy() {
        activity.supportFragmentManager.removeOnBackStackChangedListener(backStackListener)
        GbaRenderManager.detach()
        for (slot in 0 until 4) {
            InputOverrider.unregisterGba(slot)
        }
        gbaViews.forEach { binding.root.removeView(it) }
        gbaViews.clear()
    }

    fun onConfigurationChanged() {
        if (gbaViews.isNotEmpty()) {
            GbaRenderManager.updateViews(gbaViews)
            requestGbaLayout(retryLockedLandscape = true)
        }
    }

    fun handleTouch(event: MotionEvent): Boolean {
        if (isGbaLocked || isMenuOpen || gbaViews.isEmpty()) return false
        val loc = IntArray(2)
        for (gbaView in gbaViews) {
            gbaView.getLocationOnScreen(loc)
            val bounds = android.graphics.Rect(
                loc[0], loc[1],
                loc[0] + gbaView.width,
                loc[1] + gbaView.height
            )
            if (bounds.contains(event.rawX.toInt(), event.rawY.toInt())) {
                return gbaView.dispatchTouchEvent(event)
            }
        }
        return false
    }

    fun setGbaViewsTouchable(touchable: Boolean) {
        gbaViews.forEach { view ->
            view.isClickable = touchable
            view.isFocusable = touchable
            view.isFocusableInTouchMode = touchable
            if (!touchable) view.setOnTouchListener(null)
        }
    }

    fun reattachTouchListeners() {
        if (isGbaLocked) return
        gbaViews.forEach { view ->
            val slot = view.gbaSlot
            attachGbaTouchListener(view, slot, slotPrefs(slot))
        }
    }

    private fun attachGbaTouchListener(
        view: GbaOverlayView,
        slot: Int,
        slotPrefs: android.content.SharedPreferences
    ) {
        var dragX = 0f
        var dragY = 0f
        val params = view.layoutParams as FrameLayout.LayoutParams
        var cw = params.width.toFloat()
        var ch = params.height.toFloat()

        val scaleDetector = ScaleGestureDetector(
            activity,
            object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
                override fun onScale(d: ScaleGestureDetector): Boolean {
                    if (isGbaLocked) return true
                    val nativeRatio = view.aspectRatio
                    val sf = d.scaleFactor
                    val ow = cw
                    val oh = ch
                    cw = (cw * sf).coerceIn(120f, 960f)
                    ch = cw / nativeRatio
                    view.x += (ow - cw) / 2f
                    view.y += (oh - ch) / 2f

                    val p = view.layoutParams as FrameLayout.LayoutParams
                    p.width = cw.toInt()
                    p.height = ch.toInt()
                    view.layoutParams = p

                    slotPrefs.edit {
                        putFloat("gba_width", cw)
                        putFloat("gba_height", ch)
                        putFloat("gba_x", view.x)
                        putFloat("gba_y", view.y)
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
                    dragX = event.rawX - v.x
                    dragY = event.rawY - v.y
                }

                MotionEvent.ACTION_MOVE -> {
                    v.x = event.rawX - dragX
                    v.y = event.rawY - dragY
                }

                MotionEvent.ACTION_UP -> {
                    v.performClick()
                    val now = System.currentTimeMillis()
                    val last = lastGbaTapTimes[slot] ?: 0L
                    if (now - last < 300) view.onDoubleTap()
                    lastGbaTapTimes[slot] = now
                    slotPrefs.edit {
                        putFloat("gba_x", v.x)
                        putFloat("gba_y", v.y)
                    }
                }
            }
            true
        }
    }

    fun applyGbaLayout() {
        if (gbaViews.isEmpty()) return
        val rootWidth = binding.root.width
        val rootHeight = binding.root.height
        val isLandscape =
            activity.resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

        when {
            isGbaLocked && isLandscape -> applyLockedLandscapeLayout(rootWidth, rootHeight)
            isGbaLocked -> applyLockedPortraitLayout(rootWidth, rootHeight)
            else -> restoreUnlockedLayout()
        }

        activity.getSharedPreferences("gba_overlay", Context.MODE_PRIVATE).edit {
            putBoolean("gba_locked", isGbaLocked)
        }
        activeGbaSlots().forEach { applyStoredGbaVolume(it) }
    }

    private fun applyLockedLandscapeLayout(rootWidth: Int, rootHeight: Int) {
        if (!isWaitingForGcOffsetReset) {
            isWaitingForGcOffsetReset = true
            binding.root.post { applyGbaLayout() }
            return
        }
        isWaitingForGcOffsetReset = false

        val count = gbaViews.size
        val slotHeight = rootHeight / count
        val gcDrawWidth = GbaLibrary.getGCDrawWidth()
        val aspectMode = IntSetting.GFX_ASPECT_RATIO.int

        val aspectRatio = when (aspectMode) {
            1 -> 16f / 9f
            2 -> 4f / 3f
            3 -> if (rootHeight > 0) rootWidth.toFloat() / rootHeight.toFloat() else 4f / 3f
            else -> 4f / 3f
        }

        val estimatedGcWidth = (rootHeight * aspectRatio).toInt().coerceAtMost(rootWidth)
        val maxSidebarWidth = (rootWidth - estimatedGcWidth).coerceAtLeast(0)
        val rawSidebarWidth = if (gcDrawWidth > 0) {
            (rootWidth - gcDrawWidth).coerceAtLeast(0)
        } else {
            maxSidebarWidth
        }

        val gcHorizontalMargin = if (aspectMode == 1) rawSidebarWidth / 2 else 0
        var actualSidebarWidth = 0

        gbaViews.forEachIndexed { index, view ->
            view.isScreenVisible = true
            view.needsBorderRedraw = false

            val ratio = view.aspectRatio
            var targetWidth = rawSidebarWidth
            var targetHeight = (targetWidth / ratio).toInt()

            if (targetHeight > slotHeight) {
                targetHeight = slotHeight
                targetWidth = (targetHeight * ratio).toInt()
            }

            view.setBounds(
                width = targetWidth,
                height = targetHeight,
                x = 0f,
                y = (index * slotHeight).toFloat() + (slotHeight - targetHeight) / 2f
            )

            actualSidebarWidth = maxOf(actualSidebarWidth, targetWidth)
        }

        val gcOffset = if (aspectMode == 1) {
            (actualSidebarWidth - gcHorizontalMargin).coerceAtLeast(0)
        } else {
            actualSidebarWidth / 2
        }
        GbaLibrary.setGCLeftOffset(gcOffset)
    }

    private fun applyLockedPortraitLayout(rootWidth: Int, rootHeight: Int) {
        isWaitingForGcOffsetReset = false
        val count = gbaViews.size
        val gcHeight = (rootWidth * 3f / 4f).toInt()
        val gbaTop = (rootHeight - gcHeight) / 2 + gcHeight
        val availableHeight = rootHeight - gbaTop
        val maxHeight = 400

        val columns = if (count <= 2) count else 2
        val rows = if (count <= 2) 1 else 2
        val slotWidth = rootWidth / columns
        val slotHeight = availableHeight / rows

        gbaViews.forEachIndexed { index, view ->
            val ratio = view.aspectRatio
            val targetHeight = (slotWidth / ratio).toInt()
                .coerceAtMost(slotHeight)
                .coerceAtMost(maxHeight)
            val targetWidth = (targetHeight * ratio).toInt()

            val column = index % columns
            val row = index / columns

            view.setBounds(
                width = targetWidth,
                height = targetHeight,
                x = (column * slotWidth).toFloat() + (slotWidth - targetWidth) / 2f,
                y = gbaTop.toFloat() + (row * slotHeight).toFloat() + (slotHeight - targetHeight) / 2f
            )
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
        val sp = slotPrefs(slot)
        val sw = sp.getFloat("gba_width", DEFAULT_GBA_WIDTH).coerceIn(120f, 960f)
        val sh = sw / view.aspectRatio

        val screenW = activity.resources.displayMetrics.widthPixels.toFloat()
        val screenH = activity.resources.displayMetrics.heightPixels.toFloat()

        var sx = sp.getFloat("gba_x", DEFAULT_GBA_X + index * GBA_RESET_OFFSET)
        var sy = sp.getFloat("gba_y", screenH - sh - DEFAULT_GBA_X - index * GBA_RESET_OFFSET)

        if (sx < 0 || sx > screenW) sx = DEFAULT_GBA_X + index * GBA_RESET_OFFSET
        if (sy < 0 || sy > screenH) sy = screenH - sh - DEFAULT_GBA_X

        view.setBounds(sw.toInt(), sh.toInt(), sx, sy)
        attachGbaTouchListener(view, slot, sp)
    }

    fun toggleGBASnap() {
        isGbaLocked = !isGbaLocked
        if (!isGbaLocked) GbaLibrary.setGCLeftOffset(0)
        requestGbaLayout(retryLockedLandscape = true)
    }

    private fun requestGbaLayout(retryLockedLandscape: Boolean = false) {
        binding.root.post {
            applyGbaLayout()
            if (retryLockedLandscape) scheduleLockedLandscapeLayoutRetries()
        }
    }

    private fun scheduleLockedLandscapeLayoutRetries() {
        if (!isLockedLandscape()) return
        val generation = ++lockedLandscapeLayoutGeneration
        val retryDelays = longArrayOf(100L, 300L)
        retryDelays.forEach { delayMs ->
            binding.root.postDelayed({
                if (generation == lockedLandscapeLayoutGeneration && isLockedLandscape()) {
                    applyGbaLayout()
                }
            }, delayMs)
        }
    }

    private fun isLockedLandscape(): Boolean =
        isGbaLocked && activity.resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

    fun resetGBAScreens() {
        if (gbaViews.isEmpty() || isGbaLocked) return
        activity.runOnUiThread {
            val screenHeight = activity.resources.displayMetrics.heightPixels.toFloat()
            gbaViews.forEachIndexed { index, view ->
                val x = DEFAULT_GBA_X + index * GBA_RESET_OFFSET
                val y = screenHeight - DEFAULT_GBA_HEIGHT - DEFAULT_GBA_X - index * GBA_RESET_OFFSET

                slotPrefs(view.gbaSlot).edit {
                    putFloat("gba_x", x)
                    putFloat("gba_y", y)
                    putFloat("gba_width", DEFAULT_GBA_WIDTH)
                    putFloat("gba_height", DEFAULT_GBA_HEIGHT)
                }

                view.setBounds(DEFAULT_GBA_WIDTH.toInt(), DEFAULT_GBA_HEIGHT.toInt(), x, y)
            }
        }
    }

    fun resetGbaCore() {
        activeGbaSlots().forEach { GbaLibrary.resetGbaCore(it) }
    }

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
                valueFrom = 0f
                valueTo = 100f
                stepSize = 1f
                value = getGbaVolumePercent(slot).toFloat()
            }

            fun updateLabel(value: Int) {
                label.text = activity.getString(R.string.emulation_gba_volume_slot, slot + 1, value)
            }

            updateLabel(slider.value.toInt())
            slider.addOnChangeListener { _: Slider?, value: Float, _: Boolean ->
                val percent = value.toInt()
                updateLabel(percent)
                setGbaVolumePercent(slot, percent)
            }

            content.apply {
                addView(label)
                addView(slider)
            }
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
        slotPrefs(slot).getInt(GBA_VOLUME_PREF, 100).coerceIn(0, 100)

    private fun setGbaVolumePercent(slot: Int, percent: Int) {
        val clampedPercent = percent.coerceIn(0, 100)
        slotPrefs(slot).edit { putInt(GBA_VOLUME_PREF, clampedPercent) }
        setGbaVolume(slot, clampedPercent)
    }

    private fun setGbaVolume(slot: Int, percent: Int) {
        GbaLibrary.setGbaVolume(slot, percent.coerceIn(0, 100) * MIXER_MAX_VOLUME / 100)
    }

    private companion object {
        const val DEFAULT_GBA_WIDTH = 480f
        const val DEFAULT_GBA_HEIGHT = 320f
        const val DEFAULT_GBA_X = 16f
        const val GBA_RESET_OFFSET = 20f
        private const val GBA_VOLUME_PREF = "gba_volume_percent"
        private const val MIXER_MAX_VOLUME = 0xff
    }
}
