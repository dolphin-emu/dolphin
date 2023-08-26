// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import android.content.Context
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.ListItemProfileBinding

class ProfileViewHolder(
    private val presenter: ProfileDialogPresenter,
    private val binding: ListItemProfileBinding
) : RecyclerView.ViewHolder(binding.getRoot()) {
    private var profileName: String? = null
    private var stock = false

    init {
        binding.buttonLoad.setOnClickListener { loadProfile() }
        binding.buttonSave.setOnClickListener { saveProfile() }
        binding.buttonDelete.setOnClickListener { deleteProfile() }
    }

    fun bind(profileName: String, stock: Boolean) {
        this.profileName = profileName
        this.stock = stock

        binding.textName.text = profileName

        binding.buttonLoad.visibility = View.VISIBLE
        binding.buttonSave.visibility = if (stock) View.GONE else View.VISIBLE
        binding.buttonDelete.visibility = if (stock) View.GONE else View.VISIBLE
    }

    fun bindAsEmpty(context: Context) {
        profileName = null
        stock = false

        binding.textName.text = context.getText(R.string.input_profile_new)

        binding.buttonLoad.visibility = View.GONE
        binding.buttonSave.visibility = View.VISIBLE
        binding.buttonDelete.visibility = View.GONE
    }

    private fun loadProfile() = presenter.loadProfile(profileName!!, stock)

    private fun saveProfile() {
        if (profileName == null)
            presenter.saveProfileAndPromptForName()
        else
            presenter.saveProfile(profileName!!)
    }

    private fun deleteProfile() = presenter.deleteProfile(profileName!!)
}
