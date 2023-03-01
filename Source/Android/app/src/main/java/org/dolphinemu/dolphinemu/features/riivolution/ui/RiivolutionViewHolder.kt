// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.ui

import android.content.Context
import android.view.View
import android.widget.AdapterView
import android.widget.AdapterView.OnItemClickListener
import android.widget.ArrayAdapter
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ListItemRiivolutionBinding
import org.dolphinemu.dolphinemu.features.riivolution.model.RiivolutionPatches

class RiivolutionViewHolder(itemView: View, private val binding: ListItemRiivolutionBinding) :
    RecyclerView.ViewHolder(itemView), OnItemClickListener {
    private lateinit var patches: RiivolutionPatches
    private lateinit var item: RiivolutionItem

    fun bind(context: Context, patches: RiivolutionPatches, item: RiivolutionItem) {
        // TODO: Remove workaround for text filtering issue in material components when fixed
        // https://github.com/material-components/material-components-android/issues/1464
        binding.dropdownChoice.isSaveEnabled = false

        val text: String
        if (item.mOptionIndex != -1) {
            binding.textName.visibility = View.GONE
            text = patches.getOptionName(item.mDiscIndex, item.mSectionIndex, item.mOptionIndex)
            binding.layoutChoice.hint = text
        } else if (item.mSectionIndex != -1) {
            // TODO: Use supported version of setTextAppearance when min requirement is API 23
            binding.textName.setTextAppearance(context, R.style.TextAppearance_AppCompat_Medium)
            binding.layoutChoice.visibility = View.GONE
            text = patches.getSectionName(item.mDiscIndex, item.mSectionIndex)
        } else {
            binding.layoutChoice.visibility = View.GONE
            text = patches.getDiscName(item.mDiscIndex)
        }
        binding.textName.text = text

        if (item.mOptionIndex != -1) {
            this.patches = patches
            this.item = item
            val adapter = ArrayAdapter<String>(
                context,
                R.layout.support_simple_spinner_dropdown_item
            )
            val choiceCount = patches.getChoiceCount(
                item.mDiscIndex,
                item.mSectionIndex,
                item.mOptionIndex
            )
            adapter.add(context.getString(R.string.riivolution_disabled))
            for (i in 0 until choiceCount) {
                adapter.add(
                    patches.getChoiceName(
                        item.mDiscIndex,
                        item.mSectionIndex,
                        item.mOptionIndex,
                        i
                    )
                )
            }

            binding.dropdownChoice.setAdapter(adapter)
            binding.dropdownChoice.setText(
                adapter.getItem(
                    patches.getSelectedChoice(
                        item.mDiscIndex,
                        item.mSectionIndex,
                        item.mOptionIndex
                    )
                ), false
            )
            binding.dropdownChoice.onItemClickListener = this
        }
    }

    override fun onItemClick(parent: AdapterView<*>?, view: View, position: Int, id: Long) {
        patches.setSelectedChoice(
            item.mDiscIndex,
            item.mSectionIndex,
            item.mOptionIndex,
            position
        )
    }
}
