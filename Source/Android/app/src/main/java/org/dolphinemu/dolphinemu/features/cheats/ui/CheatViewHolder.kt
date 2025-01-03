// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui

import android.view.View
import android.widget.CompoundButton
import androidx.lifecycle.ViewModelProvider
import org.dolphinemu.dolphinemu.databinding.ListItemCheatBinding
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel
import org.dolphinemu.dolphinemu.utils.HapticListener

class CheatViewHolder(private val binding: ListItemCheatBinding) :
    CheatItemViewHolder(binding.getRoot()),
    View.OnClickListener,
    CompoundButton.OnCheckedChangeListener {
    private lateinit var viewModel: CheatsViewModel
    private lateinit var cheat: Cheat
    private var position = 0

    override fun bind(activity: CheatsActivity, item: CheatItem, position: Int) {
        binding.cheatSwitch.setOnCheckedChangeListener(null)
        viewModel = ViewModelProvider(activity)[CheatsViewModel::class.java]
        cheat = item.cheat!!
        this.position = position
        binding.textName.text = cheat.getName()
        binding.cheatSwitch.isChecked = cheat.getEnabled()
        binding.root.setOnClickListener(this)
        binding.cheatSwitch.setOnCheckedChangeListener(HapticListener.wrapOnCheckedChangeListener(this))
    }

    override fun onClick(root: View) {
        viewModel.setSelectedCheat(cheat, position)
        viewModel.openDetailsView()
    }

    override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
        cheat.setEnabled(isChecked)
    }
}
