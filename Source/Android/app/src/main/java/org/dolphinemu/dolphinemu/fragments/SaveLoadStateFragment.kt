// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments

import android.os.Bundle
import android.text.format.DateUtils
import android.util.SparseIntArray
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import androidx.fragment.app.Fragment
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.FragmentSaveloadStateBinding
import org.dolphinemu.dolphinemu.utils.SerializableHelper.serializable

class SaveLoadStateFragment : Fragment(), View.OnClickListener {
    enum class SaveOrLoad { SAVE, LOAD }

    private var saveOrLoad: SaveOrLoad? = null

    private var _binding: FragmentSaveloadStateBinding? = null
    private val binding get() = _binding!!

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        saveOrLoad = requireArguments().serializable(KEY_SAVEORLOAD) as SaveOrLoad?
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        _binding = FragmentSaveloadStateBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val grid = binding.gridStateSlots
        for (childIndex in 0 until grid.childCount) {
            val button = grid.getChildAt(childIndex) as Button
            setButtonText(button, childIndex)
            button.setOnClickListener(this)
        }

        // So that item clicked to start this Fragment is no longer the focused item.
        grid.requestFocus()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun onClick(view: View) {
        val buttonIndex = buttonsMap[view.id, -1]

        val action =
            (if (saveOrLoad == SaveOrLoad.SAVE) saveActionsMap else loadActionsMap)[buttonIndex]
        (requireActivity() as EmulationActivity?)?.handleMenuAction(action)

        if (saveOrLoad == SaveOrLoad.SAVE) {
            // Update the "last modified" time.
            // The savestate most likely hasn't gotten saved to disk yet (it happens asynchronously),
            // so we unfortunately can't rely on setButtonText/GetUnixTimeOfStateSlot here.
            val button = view as Button
            val time = DateUtils.getRelativeTimeSpanString(0, 0, DateUtils.MINUTE_IN_MILLIS)
            button.text = getString(R.string.emulation_state_slot, buttonIndex + 1, time)
        }
    }

    private fun setButtonText(button: Button, index: Int) {
        val creationTime = NativeLibrary.GetUnixTimeOfStateSlot(index)
        button.text = if (creationTime != 0L) {
            val relativeTime = DateUtils.getRelativeTimeSpanString(creationTime)
            getString(R.string.emulation_state_slot, index + 1, relativeTime)
        } else {
            getString(R.string.emulation_state_slot_empty, index + 1)
        }
    }

    companion object {
        private const val KEY_SAVEORLOAD = "saveorload"

        private val saveActionsMap = intArrayOf(
            EmulationActivity.MENU_ACTION_SAVE_SLOT1,
            EmulationActivity.MENU_ACTION_SAVE_SLOT2,
            EmulationActivity.MENU_ACTION_SAVE_SLOT3,
            EmulationActivity.MENU_ACTION_SAVE_SLOT4,
            EmulationActivity.MENU_ACTION_SAVE_SLOT5,
            EmulationActivity.MENU_ACTION_SAVE_SLOT6
        )

        private val loadActionsMap = intArrayOf(
            EmulationActivity.MENU_ACTION_LOAD_SLOT1,
            EmulationActivity.MENU_ACTION_LOAD_SLOT2,
            EmulationActivity.MENU_ACTION_LOAD_SLOT3,
            EmulationActivity.MENU_ACTION_LOAD_SLOT4,
            EmulationActivity.MENU_ACTION_LOAD_SLOT5,
            EmulationActivity.MENU_ACTION_LOAD_SLOT6
        )

        private val buttonsMap = SparseIntArray()

        init {
            buttonsMap.append(R.id.loadsave_state_button_1, 0)
            buttonsMap.append(R.id.loadsave_state_button_2, 1)
            buttonsMap.append(R.id.loadsave_state_button_3, 2)
            buttonsMap.append(R.id.loadsave_state_button_4, 3)
            buttonsMap.append(R.id.loadsave_state_button_5, 4)
            buttonsMap.append(R.id.loadsave_state_button_6, 5)
        }

        fun newInstance(saveOrLoad: SaveOrLoad): SaveLoadStateFragment {
            val fragment = SaveLoadStateFragment()
            val arguments = Bundle()
            arguments.putSerializable(KEY_SAVEORLOAD, saveOrLoad)
            fragment.arguments = arguments
            return fragment
        }
    }
}
