// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.databinding.ListItemProfileBinding

class ProfileAdapter(
    private val context: Context,
    private val presenter: ProfileDialogPresenter
) : RecyclerView.Adapter<ProfileViewHolder>() {
    private val stockProfileNames: Array<String> = presenter.getProfileNames(true)
    private val userProfileNames: Array<String> = presenter.getProfileNames(false)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ProfileViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val binding = ListItemProfileBinding.inflate(inflater, parent, false)
        return ProfileViewHolder(presenter, binding)
    }

    override fun onBindViewHolder(holder: ProfileViewHolder, position: Int) {
        var profilePosition = position
        if (profilePosition < stockProfileNames.size) {
            holder.bind(stockProfileNames[profilePosition], true)
            return
        }

        profilePosition -= stockProfileNames.size

        if (profilePosition < userProfileNames.size) {
            holder.bind(userProfileNames[profilePosition], false)
            return
        }

        holder.bindAsEmpty(context)
    }

    override fun getItemCount(): Int = stockProfileNames.size + userProfileNames.size + 1
}
