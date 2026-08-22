// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.adapters

import android.annotation.SuppressLint
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.FragmentActivity
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.databinding.ListItemGbaGameBinding
import org.dolphinemu.dolphinemu.features.dualscreen.GbaBootManager
import org.dolphinemu.dolphinemu.model.GbaGameFile

class GbaGameAdapter : RecyclerView.Adapter<GbaGameAdapter.GbaGameViewHolder>(),
    View.OnClickListener {
    private var gameFiles: List<GbaGameFile> = emptyList()

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GbaGameViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val binding = ListItemGbaGameBinding.inflate(inflater, parent, false)
        binding.root.setOnClickListener(this)
        return GbaGameViewHolder(binding)
    }

    override fun onBindViewHolder(holder: GbaGameViewHolder, position: Int) {
        val gameFile = gameFiles[position]
        holder.gameFile = gameFile
        holder.binding.textGbaTitle.text = gameFile.title
        holder.binding.textGbaFolder.text = gameFile.folderLabel
    }

    override fun getItemCount(): Int = gameFiles.size

    @SuppressLint("NotifyDataSetChanged")
    fun swapDataSet(nextGameFiles: List<GbaGameFile>) {
        gameFiles = nextGameFiles
        notifyDataSetChanged()
    }

    override fun onClick(view: View) {
        val holder = view.tag as GbaGameViewHolder
        GbaBootManager.launchGameBoyPlayer(view.context as FragmentActivity, holder.gameFile.path)
    }

    class GbaGameViewHolder(val binding: ListItemGbaGameBinding) :
        RecyclerView.ViewHolder(binding.root) {
        lateinit var gameFile: GbaGameFile

        init {
            binding.root.tag = this
        }
    }
}
