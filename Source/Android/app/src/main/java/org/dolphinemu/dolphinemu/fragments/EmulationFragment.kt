// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments

import android.content.Context
import android.graphics.Rect
import android.os.Bundle
import android.view.LayoutInflater
import android.view.SurfaceHolder
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.FragmentEmulationBinding
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.overlay.InputOverlay
import org.dolphinemu.dolphinemu.utils.AfterDirectoryInitializationRunner
import org.dolphinemu.dolphinemu.utils.Log
import java.io.File

class EmulationFragment : Fragment(), SurfaceHolder.Callback {
    private var inputOverlay: InputOverlay? = null

    private var gamePaths: Array<String>? = null
    private var riivolution = false
    private var runWhenSurfaceIsValid = false
    private var loadPreviousTemporaryState = false
    private var launchSystemMenu = false

    private var emulationActivity: EmulationActivity? = null

    private var _binding: FragmentEmulationBinding? = null
    private val binding get() = _binding!!

    override fun onAttach(context: Context) {
        super.onAttach(context)
        if (context is EmulationActivity) {
            emulationActivity = context
            NativeLibrary.setEmulationActivity(context)
        } else {
            throw IllegalStateException("EmulationFragment must have EmulationActivity parent")
        }
    }

    /**
     * Initialize anything that doesn't depend on the layout / views in here.
     */
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requireArguments().apply {
            gamePaths = getStringArray(KEY_GAMEPATHS)
            riivolution = getBoolean(KEY_RIIVOLUTION)
            launchSystemMenu = getBoolean(KEY_SYSTEM_MENU)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentEmulationBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        // The new Surface created here will get passed to the native code via onSurfaceChanged.
        val surfaceView = binding.surfaceEmulation
        surfaceView.holder.addCallback(this)

        inputOverlay = binding.surfaceInputOverlay

        val doneButton = binding.doneControlConfig
        doneButton?.setOnClickListener { stopConfiguringControls() }

        if (inputOverlay != null) {
            view.post {
                val overlayX = inputOverlay!!.left
                val overlayY = inputOverlay!!.top
                inputOverlay?.setSurfacePosition(
                    Rect(
                        surfaceView.left - overlayX,
                        surfaceView.top - overlayY,
                        surfaceView.right - overlayX,
                        surfaceView.bottom - overlayY
                    )
                )
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun onResume() {
        super.onResume()
        if (NativeLibrary.IsGameMetadataValid())
            inputOverlay?.refreshControls()

        AfterDirectoryInitializationRunner().runWithLifecycle(this) {
            run(emulationActivity!!.isActivityRecreated)
        }
    }

    override fun onPause() {
        if (NativeLibrary.IsRunningAndUnpaused() && !NativeLibrary.IsShowingAlertMessage()) {
            Log.debug("[EmulationFragment] Pausing emulation.")
            NativeLibrary.PauseEmulation()
        }
        super.onPause()
    }

    override fun onDestroy() {
        inputOverlay?.onDestroy()
        super.onDestroy()
    }

    override fun onDetach() {
        NativeLibrary.clearEmulationActivity()
        super.onDetach()
    }

    fun toggleInputOverlayVisibility(settings: Settings?) {
        BooleanSetting.MAIN_SHOW_INPUT_OVERLAY.setBoolean(
            settings!!,
            !BooleanSetting.MAIN_SHOW_INPUT_OVERLAY.boolean
        )

        inputOverlay?.refreshControls()
    }

    fun initInputPointer() = inputOverlay?.initTouchPointer()

    fun refreshInputOverlay() = inputOverlay?.refreshControls()

    fun refreshOverlayPointer() = inputOverlay?.refreshOverlayPointer()

    fun resetInputOverlay() = inputOverlay?.resetButtonPlacement()

    override fun surfaceCreated(holder: SurfaceHolder) {
        // We purposely don't do anything here.
        // All work is done in surfaceChanged, which we are guaranteed to get even for surface creation.
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.debug("[EmulationFragment] Surface changed. Resolution: $width x $height")
        NativeLibrary.SurfaceChanged(holder.surface)
        if (runWhenSurfaceIsValid) {
            runWithValidSurface()
        }
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.debug("[EmulationFragment] Surface destroyed.")
        NativeLibrary.SurfaceDestroyed()
        runWhenSurfaceIsValid = true
    }

    fun stopEmulation() {
        Log.debug("[EmulationFragment] Stopping emulation.")
        NativeLibrary.StopEmulation()
    }

    fun startConfiguringControls() {
        binding.doneControlConfig?.visibility = View.VISIBLE
        inputOverlay?.editMode = true
    }

    fun stopConfiguringControls() {
        binding.doneControlConfig?.visibility = View.GONE
        inputOverlay?.editMode = false
    }

    val isConfiguringControls: Boolean
        get() = inputOverlay != null && inputOverlay!!.isInEditMode

    private fun run(isActivityRecreated: Boolean) {
        if (isActivityRecreated) {
            if (NativeLibrary.IsUninitialized()) {
                loadPreviousTemporaryState = true
            } else {
                loadPreviousTemporaryState = false
                deleteFile(temporaryStateFilePath)
            }
        } else {
            Log.debug("[EmulationFragment] activity resumed or fresh start")
            loadPreviousTemporaryState = false
            // activity resumed without being killed or this is the first run
            deleteFile(temporaryStateFilePath)
        }

        // If the surface is set, run now. Otherwise, wait for it to get set.
        if (NativeLibrary.HasSurface()) {
            runWithValidSurface()
        } else {
            runWhenSurfaceIsValid = true
        }
    }

    private fun runWithValidSurface() {
        runWhenSurfaceIsValid = false
        if (NativeLibrary.IsUninitialized()) {
            NativeLibrary.SetIsBooting()
            val emulationThread = Thread({
                if (loadPreviousTemporaryState) {
                    Log.debug("[EmulationFragment] Starting emulation thread from previous state.")
                    NativeLibrary.Run(gamePaths, riivolution, temporaryStateFilePath, true)
                }
                if (launchSystemMenu) {
                    Log.debug("[EmulationFragment] Starting emulation thread for the Wii Menu.")
                    NativeLibrary.RunSystemMenu()
                } else {
                    Log.debug("[EmulationFragment] Starting emulation thread.")
                    NativeLibrary.Run(gamePaths, riivolution)
                }
                EmulationActivity.stopIgnoringLaunchRequests()
            }, "NativeEmulation")
            emulationThread.start()
        } else {
            if (!EmulationActivity.hasUserPausedEmulation && !NativeLibrary.IsShowingAlertMessage()) {
                Log.debug("[EmulationFragment] Resuming emulation.")
                NativeLibrary.UnPauseEmulation()
            }
        }
    }

    fun saveTemporaryState() = NativeLibrary.SaveStateAs(temporaryStateFilePath, true)

    private val temporaryStateFilePath: String
        get() = "${requireContext().filesDir}${File.separator}temp.sav"

    companion object {
        private const val KEY_GAMEPATHS = "gamepaths"
        private const val KEY_RIIVOLUTION = "riivolution"
        private const val KEY_SYSTEM_MENU = "systemMenu"

        fun newInstance(
            gamePaths: Array<String>?,
            riivolution: Boolean,
            systemMenu: Boolean
        ): EmulationFragment {
            val args = Bundle()
            args.apply {
                putStringArray(KEY_GAMEPATHS, gamePaths)
                putBoolean(KEY_RIIVOLUTION, riivolution)
                putBoolean(KEY_SYSTEM_MENU, systemMenu)
            }
            val fragment = EmulationFragment()
            fragment.arguments = args
            return fragment
        }

        private fun deleteFile(path: String) {
            try {
                val file = File(path)
                if (!file.delete()) {
                    Log.error("[EmulationFragment] Failed to delete ${file.absolutePath}")
                }
            } catch (ignored: Exception) {
            }
        }
    }
}
