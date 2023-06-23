// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.infinitybase.ui

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
import org.dolphinemu.dolphinemu.databinding.DialogCreateInfinityFigureBinding
import org.dolphinemu.dolphinemu.databinding.ListItemNfcFigureSlotBinding
import org.dolphinemu.dolphinemu.features.infinitybase.InfinityConfig
import org.dolphinemu.dolphinemu.features.infinitybase.InfinityConfig.removeFigure

class FigureSlotAdapter(
    private val figures: List<FigureSlot>,
    private val activity: EmulationActivity
) : RecyclerView.Adapter<FigureSlotAdapter.ViewHolder>(),
    OnItemClickListener {

    class ViewHolder(var binding: ListItemNfcFigureSlotBinding) :
        RecyclerView.ViewHolder(binding.getRoot())

    private lateinit var binding: DialogCreateInfinityFigureBinding

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): ViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val binding = ListItemNfcFigureSlotBinding.inflate(inflater, parent, false)
        return ViewHolder(binding)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val figure = figures[position]
        holder.binding.textFigureName.text = figure.label

        holder.binding.buttonClearFigure.setOnClickListener {
            removeFigure(figure.position)
            activity.clearInfinityFigure(position)
        }

        holder.binding.buttonLoadFigure.setOnClickListener {
            val loadFigure = Intent(Intent.ACTION_OPEN_DOCUMENT)
            loadFigure.addCategory(Intent.CATEGORY_OPENABLE)
            loadFigure.type = "*/*"
            activity.setInfinityFigureData(0, "", figure.position, position)
            activity.startActivityForResult(
                loadFigure,
                EmulationActivity.REQUEST_INFINITY_FIGURE_FILE
            )
        }

        val inflater = LayoutInflater.from(activity)
        binding = DialogCreateInfinityFigureBinding.inflate(inflater)

        binding.infinityDropdown.onItemClickListener = this

        holder.binding.buttonCreateFigure.setOnClickListener {
            var validFigures = InfinityConfig.REVERSE_LIST_FIGURES
            // Filter adapter list by position, either Hexagon Pieces, Characters or Abilities
            validFigures = when (figure.position) {
                0 -> {
                    // Hexagon Pieces
                    validFigures.filter { (_, value) -> value in 2000000..2999999 || value in 4000000..4999999 }
                }

                1, 2 -> {
                    // Characters
                    validFigures.filter { (_, value) -> value in 1000000..1999999 }
                }

                else -> {
                    // Abilities
                    validFigures.filter { (_, value) -> value in 3000000..3999999 }
                }
            }
            val figureListKeys = validFigures.keys.toMutableList()
            figureListKeys.sort()
            val figureNames: ArrayList<String> = ArrayList(figureListKeys)
            binding.infinityDropdown.setAdapter(
                ArrayAdapter(
                    activity, R.layout.support_simple_spinner_dropdown_item,
                    figureNames
                )
            )

            if (binding.getRoot().parent != null) {
                (binding.getRoot().parent as ViewGroup).removeAllViews()
            }
            val createDialog = MaterialAlertDialogBuilder(activity)
                .setTitle(R.string.create_figure_title)
                .setView(binding.getRoot())
                .setPositiveButton(R.string.create_figure, null)
                .setNegativeButton(R.string.cancel, null)
                .show()
            createDialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                if (binding.infinityNum.text.toString().isNotBlank()) {
                    val createFigure = Intent(Intent.ACTION_CREATE_DOCUMENT)
                    createFigure.addCategory(Intent.CATEGORY_OPENABLE)
                    createFigure.type = "*/*"
                    val num = binding.infinityNum.text.toString().toLong()
                    val name = InfinityConfig.LIST_FIGURES[num]
                    if (name != null) {
                        createFigure.putExtra(
                            Intent.EXTRA_TITLE,
                            "$name.bin"
                        )
                        activity.setInfinityFigureData(num, name, figure.position, position)
                    } else {
                        createFigure.putExtra(
                            Intent.EXTRA_TITLE,
                            "Unknown(Number: $num).bin"
                        )
                        activity.setInfinityFigureData(num, "Unknown", figure.position, position)
                    }
                    activity.startActivityForResult(
                        createFigure,
                        EmulationActivity.REQUEST_CREATE_INFINITY_FIGURE
                    )
                    createDialog.dismiss()
                } else {
                    Toast.makeText(
                        activity, R.string.invalid_infinity_figure,
                        Toast.LENGTH_SHORT
                    ).show()
                }
            }
        }
    }

    override fun getItemCount(): Int {
        return figures.size
    }

    override fun onItemClick(parent: AdapterView<*>, view: View, position: Int, id: Long) {
        val figureNumber = InfinityConfig.REVERSE_LIST_FIGURES[parent.getItemAtPosition(position)]
        binding.infinityNum.setText(figureNumber.toString())
    }
}
