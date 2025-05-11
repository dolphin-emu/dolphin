package org.dolphinemu.dolphinemu.fragments

import android.app.Activity
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import android.view.LayoutInflater
import android.view.ViewGroup
import android.os.Bundle
import android.view.View
import com.google.android.material.bottomsheet.BottomSheetBehavior
import org.dolphinemu.dolphinemu.features.settings.model.BooleanSetting
import android.widget.CompoundButton
import androidx.appcompat.app.AppCompatActivity
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.FragmentGridOptionsBinding
import org.dolphinemu.dolphinemu.databinding.FragmentGridOptionsTvBinding
import org.dolphinemu.dolphinemu.features.settings.model.NativeConfig
import org.dolphinemu.dolphinemu.ui.main.MainView
import org.dolphinemu.dolphinemu.utils.HapticListener

class GridOptionDialogFragment : BottomSheetDialogFragment() {

    private lateinit var mView: MainView

    private var _mBindingMobile: FragmentGridOptionsBinding? = null
    private var _mBindingTv: FragmentGridOptionsTvBinding? = null

    // This property is only valid between onCreateView and
    // onDestroyView.
    private val mBindingMobile get() = _mBindingMobile!!
    private val mBindingTv get() = _mBindingTv!!

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        mView = (activity as MainView)

        if (activity is AppCompatActivity)
        {
            _mBindingMobile = FragmentGridOptionsBinding.inflate(inflater, container, false)
            return mBindingMobile.root
        }
        _mBindingTv = FragmentGridOptionsTvBinding.inflate(inflater, container, false)
        return mBindingTv.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        // Pins fragment to the top of the dialog ensures the dialog is expanded in landscape by default
        if (!resources.getBoolean(R.bool.hasTouch)) {
            BottomSheetBehavior.from<View>(view.parent as View).state =
                BottomSheetBehavior.STATE_EXPANDED
        }

        if (activity is AppCompatActivity) {
            setUpCoverButtons()
            setUpTitleButtons()
        } else {
            setUpCoverButtonsTv()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _mBindingMobile = null
        _mBindingTv = null
    }

    private fun setUpCoverButtons() {
        mBindingMobile.switchDownloadCovers.isChecked = BooleanSetting.MAIN_USE_GAME_COVERS.boolean
        mBindingMobile.rootDownloadCovers.setOnClickListener {
            mBindingMobile.switchDownloadCovers.isChecked = !mBindingMobile.switchDownloadCovers.isChecked
        }
        mBindingMobile.switchDownloadCovers.setOnCheckedChangeListener(
            HapticListener.wrapOnCheckedChangeListener { _: CompoundButton, _: Boolean ->
            BooleanSetting.MAIN_USE_GAME_COVERS.setBoolean(
                NativeConfig.LAYER_BASE,
                mBindingMobile.switchDownloadCovers.isChecked
            )
            (mView as Activity).recreate()
        })
    }

    private fun setUpTitleButtons() {
        mBindingMobile.switchShowTitles.isChecked = BooleanSetting.MAIN_SHOW_GAME_TITLES.boolean
        mBindingMobile.rootShowTitles.setOnClickListener {
            mBindingMobile.switchShowTitles.isChecked = !mBindingMobile.switchShowTitles.isChecked
        }
        mBindingMobile.switchShowTitles.setOnCheckedChangeListener(
            HapticListener.wrapOnCheckedChangeListener { _: CompoundButton, _: Boolean ->
            BooleanSetting.MAIN_SHOW_GAME_TITLES.setBoolean(
                NativeConfig.LAYER_BASE,
                mBindingMobile.switchShowTitles.isChecked
            )
            mView.reloadGrid()
        })
    }

    // TODO: Remove this when leanback is removed
    private fun setUpCoverButtonsTv() {
        mBindingTv.switchDownloadCovers.isChecked =
            BooleanSetting.MAIN_USE_GAME_COVERS.boolean
        mBindingTv.rootDownloadCovers.setOnClickListener {
            mBindingTv.switchDownloadCovers.isChecked = !mBindingTv.switchDownloadCovers.isChecked
        }
        mBindingTv.switchDownloadCovers.setOnCheckedChangeListener { _: CompoundButton, _: Boolean ->
            BooleanSetting.MAIN_USE_GAME_COVERS.setBoolean(
                NativeConfig.LAYER_BASE,
                mBindingTv.switchDownloadCovers.isChecked
            )
            (mView as Activity).recreate()
        }
    }
}
