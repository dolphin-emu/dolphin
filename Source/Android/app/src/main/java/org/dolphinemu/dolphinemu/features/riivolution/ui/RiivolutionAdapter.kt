// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.riivolution.ui

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.databinding.ListItemRiivolutionBinding
import org.dolphinemu.dolphinemu.features.riivolution.model.RiivolutionPatches

class RiivolutionAdapter(private val context: Context, private val patches: RiivolutionPatches) :
    RecyclerView.Adapter<RiivolutionViewHolder>() {
    private val items = ArrayList<RiivolutionItem>()

    init {
        val discCount = patches.getDiscCount()
        for (i in 0 until discCount) {
            items.add(RiivolutionItem(i))

            val sectionCount = patches.getSectionCount(i)
            for (j in 0 until sectionCount) {
                items.add(RiivolutionItem(i, j))

                val optionCount = patches.getOptionCount(i, j)
                for (k in 0 until optionCount) {
                    items.add(RiivolutionItem(i, j, k))
                }
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RiivolutionViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val binding = ListItemRiivolutionBinding.inflate(inflater)
        return RiivolutionViewHolder(binding.root, binding)
    }

    override fun onBindViewHolder(holder: RiivolutionViewHolder, position: Int) {
        holder.bind(context, patches, items[position])
    }

    override fun getItemCount(): Int {
        return items.size
    }
}
