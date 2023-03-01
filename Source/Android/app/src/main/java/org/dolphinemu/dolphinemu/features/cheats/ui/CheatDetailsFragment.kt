// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.cheats.ui

import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.FragmentCheatDetailsBinding
import org.dolphinemu.dolphinemu.features.cheats.model.Cheat
import org.dolphinemu.dolphinemu.features.cheats.model.CheatsViewModel

class CheatDetailsFragment : Fragment() {
    private lateinit var viewModel: CheatsViewModel
    private var cheat: Cheat? = null

    private var _binding: FragmentCheatDetailsBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentCheatDetailsBinding.inflate(inflater, container, false)
        return binding.getRoot()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val activity = requireActivity() as CheatsActivity
        viewModel = ViewModelProvider(activity)[CheatsViewModel::class.java]

        viewModel.selectedCheat.observe(viewLifecycleOwner) { cheat: Cheat? ->
            onSelectedCheatUpdated(
                cheat
            )
        }
        viewModel.isEditing.observe(viewLifecycleOwner) { isEditing: Boolean ->
            onIsEditingUpdated(
                isEditing
            )
        }

        binding.buttonDelete.setOnClickListener { onDeleteClicked() }
        binding.buttonEdit.setOnClickListener { onEditClicked() }
        binding.buttonCancel.setOnClickListener { onCancelClicked() }
        binding.buttonOk.setOnClickListener { onOkClicked() }

        CheatsActivity.setOnFocusChangeListenerRecursively(
            view
        ) { _: View?, hasFocus: Boolean -> activity.onDetailsViewFocusChange(hasFocus) }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun clearEditErrors() {
        binding.editName.error = null
        binding.editCode.error = null
    }

    private fun onDeleteClicked() {
        MaterialAlertDialogBuilder(requireContext())
            .setMessage(getString(R.string.cheats_delete_confirmation, cheat!!.getName()))
            .setPositiveButton(R.string.yes) { _: DialogInterface?, i: Int -> viewModel.deleteSelectedCheat() }
            .setNegativeButton(R.string.no, null)
            .show()
    }

    private fun onEditClicked() {
        viewModel.setIsEditing(true)
        binding.buttonOk.requestFocus()
    }

    private fun onCancelClicked() {
        viewModel.setIsEditing(false)
        onSelectedCheatUpdated(cheat)
        binding.buttonDelete.requestFocus()
    }

    private fun onOkClicked() {
        clearEditErrors()

        val result = cheat!!.setCheat(
            binding.editNameInput.text.toString(),
            binding.editCreatorInput.text.toString(),
            binding.editNotesInput.text.toString(),
            binding.editCodeInput.text.toString()
        )

        when (result) {
            Cheat.TRY_SET_SUCCESS -> {
                if (viewModel.isAdding.value!!) {
                    viewModel.finishAddingCheat()
                    onSelectedCheatUpdated(cheat)
                } else {
                    viewModel.notifySelectedCheatChanged()
                    viewModel.setIsEditing(false)
                }
                binding.buttonEdit.requestFocus()
            }
            Cheat.TRY_SET_FAIL_NO_NAME -> {
                binding.editName.error = getString(R.string.cheats_error_no_name)
                binding.scrollView.smoothScrollTo(0, binding.editNameInput.top)
            }
            Cheat.TRY_SET_FAIL_NO_CODE_LINES -> {
                binding.editCode.error = getString(R.string.cheats_error_no_code_lines)
                binding.scrollView.smoothScrollTo(0, binding.editCodeInput.bottom)
            }
            Cheat.TRY_SET_FAIL_CODE_MIXED_ENCRYPTION -> {
                binding.editCode.error = getString(R.string.cheats_error_mixed_encryption)
                binding.scrollView.smoothScrollTo(0, binding.editCodeInput.bottom)
            }
            else -> {
                binding.editCode.error = getString(R.string.cheats_error_on_line, result)
                binding.scrollView.smoothScrollTo(0, binding.editCodeInput.bottom)
            }
        }
    }

    private fun onSelectedCheatUpdated(cheat: Cheat?) {
        clearEditErrors()

        binding.root.visibility = if (cheat == null) View.GONE else View.VISIBLE

        val creatorVisibility =
            if (cheat != null && cheat.supportsCreator()) View.VISIBLE else View.GONE
        val notesVisibility =
            if (cheat != null && cheat.supportsNotes()) View.VISIBLE else View.GONE
        val codeVisibility = if (cheat != null && cheat.supportsCode()) View.VISIBLE else View.GONE
        binding.editCreator.visibility = creatorVisibility
        binding.editNotes.visibility = notesVisibility
        binding.editCode.visibility = codeVisibility

        val userDefined = cheat != null && cheat.getUserDefined()
        binding.buttonDelete.isEnabled = userDefined
        binding.buttonEdit.isEnabled = userDefined

        // If the fragment was recreated while editing a cheat, it's vital that we
        // don't repopulate the fields, otherwise the user's changes will be lost
        val isEditing = viewModel.isEditing.value!!

        if (!isEditing && cheat != null) {
            binding.editNameInput.setText(cheat.getName())
            binding.editCreatorInput.setText(cheat.getCreator())
            binding.editNotesInput.setText(cheat.getNotes())
            binding.editCodeInput.setText(cheat.getCode())
        }
        this.cheat = cheat
    }

    private fun onIsEditingUpdated(isEditing: Boolean) {
        binding.editNameInput.isEnabled = isEditing
        binding.editCreatorInput.isEnabled = isEditing
        binding.editNotesInput.isEnabled = isEditing
        binding.editCodeInput.isEnabled = isEditing

        binding.buttonDelete.visibility = if (isEditing) View.GONE else View.VISIBLE
        binding.buttonEdit.visibility = if (isEditing) View.GONE else View.VISIBLE
        binding.buttonCancel.visibility = if (isEditing) View.VISIBLE else View.GONE
        binding.buttonOk.visibility = if (isEditing) View.VISIBLE else View.GONE
    }
}
