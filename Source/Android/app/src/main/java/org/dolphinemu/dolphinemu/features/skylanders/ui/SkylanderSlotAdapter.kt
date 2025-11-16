// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders.ui

import android.app.AlertDialog
import android.content.Intent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.AdapterView.OnItemClickListener
import android.widget.ArrayAdapter
import android.widget.Toast
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.databinding.DialogCreateSkylanderBinding
import org.dolphinemu.dolphinemu.databinding.ListItemNfcFigureSlotBinding
import org.dolphinemu.dolphinemu.features.skylanders.SkylanderConfig
import org.dolphinemu.dolphinemu.features.skylanders.SkylanderConfig.removeSkylander
import org.dolphinemu.dolphinemu.features.skylanders.model.SkylanderPair

class SkylanderSlotAdapter(
    private val slots: List<SkylanderSlot>,
    private val activity: EmulationActivity
) : RecyclerView.Adapter<SkylanderSlotAdapter.ViewHolder>(), OnItemClickListener {
    class ViewHolder(var binding: ListItemNfcFigureSlotBinding) :
        RecyclerView.ViewHolder(binding.getRoot())

    private lateinit var binding: DialogCreateSkylanderBinding

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val binding = ListItemNfcFigureSlotBinding.inflate(inflater, parent, false)
        return ViewHolder(binding)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val slot = slots[position]
        holder.binding.textFigureName.text = slot.label

        holder.binding.buttonClearFigure.setOnClickListener {
            removeSkylander(slot.portalSlot)
            activity.clearSkylander(slot.slotNum)
        }

        holder.binding.buttonLoadFigure.setOnClickListener {
            activity.setSkylanderData(0, 0, "", position)
            activity.requestSkylanderFile.launch("*/*")
        }

        val inflater = LayoutInflater.from(activity)
        binding = DialogCreateSkylanderBinding.inflate(inflater)

        val nameList = SkylanderConfig.REVERSE_LIST_SKYLANDERS.keys.toMutableList()
        nameList.sort()
        val skylanderNames: ArrayList<String> = ArrayList(nameList)

        binding.skylanderDropdown.setAdapter(
            ArrayAdapter(
                activity, R.layout.support_simple_spinner_dropdown_item,
                skylanderNames
            )
        )
        binding.skylanderDropdown.onItemClickListener = this

        holder.binding.buttonCreateFigure.setOnClickListener {
            if (binding.getRoot().parent != null) {
                (binding.getRoot().parent as ViewGroup).removeAllViews()
            }
            val createDialog = MaterialAlertDialogBuilder(activity)
                .setTitle(R.string.create_skylander_title)
                .setView(binding.getRoot())
                .setPositiveButton(R.string.create_figure, null)
                .setNegativeButton(R.string.cancel, null)
                .show()
            createDialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                if (binding.skylanderId.text.toString().isNotBlank() &&
                    binding.skylanderVar.text.toString().isNotBlank()
                ) {
                    val id = binding.skylanderId.text.toString().toInt()
                    val variant = binding.skylanderVar.text.toString().toInt()
                    val name = SkylanderConfig.LIST_SKYLANDERS[SkylanderPair(id, variant)]
                    val title = if (name != null) "$name.sky" else "Unknown(ID: $id Variant: $variant).sky"
                    activity.setSkylanderData(id, variant, name ?: "Unknown", position)
                    activity.requestCreateSkylander.launch(title)
                    createDialog.dismiss()
                } else {
                    Toast.makeText(
                        activity, R.string.invalid_skylander,
                        Toast.LENGTH_SHORT
                    ).show()
                }
            }
        }
    }

    override fun getItemCount(): Int {
        return slots.size
    }

    override fun onItemClick(parent: AdapterView<*>, view: View, position: Int, id: Long) {
        val skylanderIdVar =
            SkylanderConfig.REVERSE_LIST_SKYLANDERS[parent.getItemAtPosition(position)]
        binding.skylanderId.setText(skylanderIdVar!!.id.toString())
        binding.skylanderVar.setText(skylanderIdVar.variant.toString())
    }
}
