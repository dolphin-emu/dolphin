// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui

import android.view.View
import android.widget.TextView
import androidx.lifecycle.ViewModelProvider
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ListItemSubmenuBinding
import org.dolphinemu.dolphinemu.features.cheats.model.ARCheat
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel
import org.dolphinemu.dolphinemu.features.cheats.model.GeckoCheat
import org.dolphinemu.dolphinemu.features.cheats.model.PatchCheat

class ActionViewHolder(binding: ListItemSubmenuBinding) : CheatItemViewHolder(binding.root),
    View.OnClickListener {
    private val mName: TextView

    private lateinit var activity: CheatsActivity
    private lateinit var viewModel: CheatsViewModel
    private var string = 0
    private var position = 0

    init {
        mName = binding.textSettingName
        binding.root.setOnClickListener(this)
    }

    override fun bind(activity: CheatsActivity, item: CheatItem, position: Int) {
        this.activity = activity
        viewModel = ViewModelProvider(this.activity)[CheatsViewModel::class.java]
        string = item.string
        this.position = position
        mName.setText(string)
    }

    override fun onClick(root: View) {
        when(string) {
            R.string.cheats_add_ar -> {
                viewModel.startAddingCheat(ARCheat(), position)
                viewModel.openDetailsView()
            }
            R.string.cheats_add_gecko -> {
                viewModel.startAddingCheat(GeckoCheat(), position)
                viewModel.openDetailsView()
            }
            R.string.cheats_add_patch -> {
                viewModel.startAddingCheat(PatchCheat(), position)
                viewModel.openDetailsView()
            }
            R.string.cheats_download_gecko -> activity.downloadGeckoCodes()
        }
    }
}
