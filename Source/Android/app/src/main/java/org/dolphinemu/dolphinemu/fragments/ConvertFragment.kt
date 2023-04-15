// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments

import android.app.Activity
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.provider.DocumentsContract
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.AdapterView.OnItemClickListener
import android.widget.ArrayAdapter
import androidx.annotation.StringRes
import androidx.fragment.app.Fragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.textfield.MaterialAutoCompleteTextView
import com.google.android.material.textfield.TextInputLayout
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.DialogProgressBinding
import org.dolphinemu.dolphinemu.databinding.FragmentConvertBinding
import org.dolphinemu.dolphinemu.model.GameFile
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.ui.platform.Platform
import java.io.File

class ConvertFragment : Fragment(), View.OnClickListener {
    private class DropdownValue : OnItemClickListener {
        private var valuesId = -1
        var position = 0
            set(position) {
                field = position
                for (callback in callbacks) callback.run()
            }
        private val callbacks = ArrayList<Runnable>()

        override fun onItemClick(parent: AdapterView<*>?, view: View, position: Int, id: Long) {
            if (this.position != position) this.position = position
        }

        fun populate(valuesId: Int) {
            this.valuesId = valuesId
        }

        fun hasValues(): Boolean {
            return valuesId != -1
        }

        fun getValue(context: Context): Int {
            return context.resources.getIntArray(valuesId)[position]
        }

        fun getValueOr(context: Context, defaultValue: Int): Int {
            return if (hasValues()) getValue(context) else defaultValue
        }

        fun addCallback(callback: Runnable) {
            callbacks.add(callback)
        }
    }

    private val format = DropdownValue()
    private val blockSize = DropdownValue()
    private val compression = DropdownValue()
    private val compressionLevel = DropdownValue()
    private lateinit var gameFile: GameFile

    @Volatile
    private var canceled = false

    @Volatile
    private var thread: Thread? = null

    private var _binding: FragmentConvertBinding? = null
    val binding get() = _binding!!

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        gameFile = GameFileCacheManager.addOrGet(requireArguments().getString(ARG_GAME_PATH))
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentConvertBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        // TODO: Remove workaround for text filtering issue in material components when fixed
        // https://github.com/material-components/material-components-android/issues/1464
        binding.dropdownFormat.isSaveEnabled = false
        binding.dropdownBlockSize.isSaveEnabled = false
        binding.dropdownCompression.isSaveEnabled = false
        binding.dropdownCompressionLevel.isSaveEnabled = false

        populateFormats()
        populateBlockSize()
        populateCompression()
        populateCompressionLevel()
        populateRemoveJunkData()

        format.addCallback { populateBlockSize() }
        format.addCallback { populateCompression() }
        format.addCallback { populateCompressionLevel() }
        compression.addCallback { populateCompressionLevel() }
        format.addCallback { populateRemoveJunkData() }

        binding.buttonConvert.setOnClickListener(this)

        if (savedInstanceState != null) {
            setDropdownSelection(
                binding.dropdownFormat, format, savedInstanceState.getInt(
                    KEY_FORMAT
                )
            )
            setDropdownSelection(
                binding.dropdownBlockSize, blockSize,
                savedInstanceState.getInt(KEY_BLOCK_SIZE)
            )
            setDropdownSelection(
                binding.dropdownCompression, compression,
                savedInstanceState.getInt(KEY_COMPRESSION)
            )
            setDropdownSelection(
                binding.dropdownCompressionLevel, compressionLevel,
                savedInstanceState.getInt(KEY_COMPRESSION_LEVEL)
            )
            binding.switchRemoveJunkData.isChecked = savedInstanceState.getBoolean(
                KEY_REMOVE_JUNK_DATA
            )
        }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putInt(KEY_FORMAT, format.position)
        outState.putInt(KEY_BLOCK_SIZE, blockSize.position)
        outState.putInt(KEY_COMPRESSION, compression.position)
        outState.putInt(KEY_COMPRESSION_LEVEL, compressionLevel.position)
        outState.putBoolean(KEY_REMOVE_JUNK_DATA, binding.switchRemoveJunkData.isChecked)
    }

    private fun setDropdownSelection(
        dropdown: MaterialAutoCompleteTextView,
        valueWrapper: DropdownValue,
        selection: Int
    ) {
        if (dropdown.adapter != null) {
            dropdown.setText(dropdown.adapter.getItem(selection).toString(), false)
        }
        valueWrapper.position = selection
    }

    override fun onStop() {
        super.onStop()
        canceled = true
        joinThread()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun populateDropdown(
        layout: TextInputLayout,
        dropdown: MaterialAutoCompleteTextView,
        entriesId: Int,
        valuesId: Int,
        valueWrapper: DropdownValue
    ) {
        val adapter = ArrayAdapter.createFromResource(
            requireContext(),
            entriesId,
            R.layout.support_simple_spinner_dropdown_item
        )
        dropdown.setAdapter(adapter)

        valueWrapper.populate(valuesId)
        dropdown.onItemClickListener = valueWrapper

        layout.isEnabled = adapter.count > 1
    }

    private fun clearDropdown(
        layout: TextInputLayout,
        dropdown: MaterialAutoCompleteTextView,
        valueWrapper: DropdownValue
    ) {
        dropdown.setAdapter(null)
        layout.isEnabled = false

        valueWrapper.populate(-1)
        valueWrapper.position = 0
        dropdown.setText(null, false)
        dropdown.onItemClickListener = valueWrapper
    }

    private fun populateFormats() {
        populateDropdown(
            binding.format,
            binding.dropdownFormat,
            R.array.convertFormatEntries,
            R.array.convertFormatValues,
            format
        )
        if (gameFile.blobType == BLOB_TYPE_ISO) {
            setDropdownSelection(
                binding.dropdownFormat,
                format,
                binding.dropdownFormat.adapter.count - 1
            )
        }
        binding.dropdownFormat.setText(
            binding.dropdownFormat.adapter.getItem(format.position).toString(), false
        )
    }

    private fun populateBlockSize() {
        when (format.getValue(requireContext())) {
            BLOB_TYPE_GCZ -> {
                // In the equivalent DolphinQt code, we have some logic for avoiding block sizes that can
                // trigger bugs in Dolphin versions older than 5.0-11893, but it was too annoying to port.
                // TODO: Port it?
                populateDropdown(
                    binding.blockSize,
                    binding.dropdownBlockSize,
                    R.array.convertBlockSizeGczEntries,
                    R.array.convertBlockSizeGczValues,
                    blockSize
                )
                blockSize.position = 0
                binding.dropdownBlockSize.setText(
                    binding.dropdownBlockSize.adapter.getItem(0).toString(), false
                )
            }
            BLOB_TYPE_WIA -> {
                populateDropdown(
                    binding.blockSize,
                    binding.dropdownBlockSize,
                    R.array.convertBlockSizeWiaEntries,
                    R.array.convertBlockSizeWiaValues,
                    blockSize
                )
                blockSize.position = 0
                binding.dropdownBlockSize.setText(
                    binding.dropdownBlockSize.adapter.getItem(0).toString(), false
                )
            }
            BLOB_TYPE_RVZ -> {
                populateDropdown(
                    binding.blockSize,
                    binding.dropdownBlockSize,
                    R.array.convertBlockSizeRvzEntries,
                    R.array.convertBlockSizeRvzValues,
                    blockSize
                )
                blockSize.position = 2
                binding.dropdownBlockSize.setText(
                    binding.dropdownBlockSize.adapter.getItem(2).toString(), false
                )
            }
            else -> clearDropdown(binding.blockSize, binding.dropdownBlockSize, blockSize)
        }
    }

    private fun populateCompression() {
        when (format.getValue(requireContext())) {
            BLOB_TYPE_GCZ -> {
                populateDropdown(
                    binding.compression,
                    binding.dropdownCompression,
                    R.array.convertCompressionGczEntries,
                    R.array.convertCompressionGczValues,
                    compression
                )
                compression.position = 0
                binding.dropdownCompression.setText(
                    binding.dropdownCompression.adapter.getItem(0).toString(), false
                )
            }
            BLOB_TYPE_WIA -> {
                populateDropdown(
                    binding.compression,
                    binding.dropdownCompression,
                    R.array.convertCompressionWiaEntries,
                    R.array.convertCompressionWiaValues,
                    compression
                )
                compression.position = 0
                binding.dropdownCompression.setText(
                    binding.dropdownCompression.adapter.getItem(0).toString(), false
                )
            }
            BLOB_TYPE_RVZ -> {
                populateDropdown(
                    binding.compression,
                    binding.dropdownCompression,
                    R.array.convertCompressionRvzEntries,
                    R.array.convertCompressionRvzValues,
                    compression
                )
                compression.position = 4
                binding.dropdownCompression.setText(
                    binding.dropdownCompression.adapter.getItem(4).toString(), false
                )
            }
            else -> clearDropdown(
                binding.compression,
                binding.dropdownCompression,
                compression
            )
        }
    }

    private fun populateCompressionLevel() {
        when (compression.getValueOr(requireContext(), COMPRESSION_NONE)) {
            COMPRESSION_BZIP2, COMPRESSION_LZMA, COMPRESSION_LZMA2 -> {
                populateDropdown(
                    binding.compressionLevel,
                    binding.dropdownCompressionLevel,
                    R.array.convertCompressionLevelEntries,
                    R.array.convertCompressionLevelValues,
                    compressionLevel
                )
                compressionLevel.position = 4
                binding.dropdownCompressionLevel.setText(
                    binding.dropdownCompressionLevel.adapter.getItem(
                        4
                    ).toString(), false
                )
            }
            COMPRESSION_ZSTD -> {
                // TODO: Query DiscIO for the supported compression levels, like we do in DolphinQt?
                populateDropdown(
                    binding.compressionLevel,
                    binding.dropdownCompressionLevel,
                    R.array.convertCompressionLevelZstdEntries,
                    R.array.convertCompressionLevelZstdValues,
                    compressionLevel
                )
                compressionLevel.position = 4
                binding.dropdownCompressionLevel.setText(
                    binding.dropdownCompressionLevel.adapter.getItem(
                        4
                    ).toString(), false
                )
            }
            else -> clearDropdown(
                binding.compressionLevel,
                binding.dropdownCompressionLevel,
                compressionLevel
            )
        }
    }

    private fun populateRemoveJunkData() {
        val scrubbingAllowed = format.getValue(requireContext()) != BLOB_TYPE_RVZ &&
                !gameFile.isDatelDisc

        binding.switchRemoveJunkData.isEnabled = scrubbingAllowed
        if (!scrubbingAllowed) binding.switchRemoveJunkData.isChecked = false
    }

    override fun onClick(view: View) {
        val scrub = binding.switchRemoveJunkData.isChecked
        val format = format.getValue(requireContext())

        var action = Runnable { showSavePrompt() }

        if (gameFile.isNKit) {
            action = addAreYouSureDialog(action, R.string.convert_warning_nkit)
        }

        if (!scrub && format == BLOB_TYPE_GCZ && !gameFile.isDatelDisc && gameFile.platform == Platform.WII.toInt()) {
            action = addAreYouSureDialog(action, R.string.convert_warning_gcz)
        }

        if (scrub && format == BLOB_TYPE_ISO) {
            action = addAreYouSureDialog(action, R.string.convert_warning_iso)
        }

        action.run()
    }

    private fun addAreYouSureDialog(action: Runnable, @StringRes warning_text: Int): Runnable {
        return Runnable {
            val context = requireContext()
            MaterialAlertDialogBuilder(context)
                .setMessage(warning_text)
                .setPositiveButton(R.string.yes) { _: DialogInterface?, _: Int -> action.run() }
                .setNegativeButton(R.string.no, null)
                .show()
        }
    }

    private fun showSavePrompt() {
        val originalPath = gameFile.path

        val filename = StringBuilder(File(originalPath).name)
        val dotIndex = filename.lastIndexOf(".")
        if (dotIndex != -1) filename.setLength(dotIndex)
        when (format.getValue(requireContext())) {
            BLOB_TYPE_ISO -> filename.append(".iso")
            BLOB_TYPE_GCZ -> filename.append(".gcz")
            BLOB_TYPE_WIA -> filename.append(".wia")
            BLOB_TYPE_RVZ -> filename.append(".rvz")
        }

        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT)
        intent.addCategory(Intent.CATEGORY_OPENABLE)
        intent.type = "application/octet-stream"
        intent.putExtra(Intent.EXTRA_TITLE, filename.toString())
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) intent.putExtra(
            DocumentsContract.EXTRA_INITIAL_URI,
            originalPath
        )
        startActivityForResult(intent, REQUEST_CODE_SAVE_FILE)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == REQUEST_CODE_SAVE_FILE && resultCode == Activity.RESULT_OK) {
            convert(data!!.data.toString())
        }
    }

    private fun convert(outPath: String) {
        val context = requireContext()

        joinThread()

        canceled = false

        val dialogProgressBinding = DialogProgressBinding.inflate(layoutInflater, null, false)
        dialogProgressBinding.updateProgress.max = PROGRESS_RESOLUTION

        val progressDialog = MaterialAlertDialogBuilder(context)
            .setTitle(R.string.convert_converting)
            .setOnCancelListener { canceled = true }
            .setNegativeButton(getString(R.string.cancel)) { dialog: DialogInterface, _: Int -> dialog.dismiss() }
            .setView(dialogProgressBinding.root)
            .show()

        thread = Thread {
            val success = NativeLibrary.ConvertDiscImage(
                gameFile.path,
                outPath,
                gameFile.platform,
                format.getValue(context),
                blockSize.getValueOr(context, 0),
                compression.getValueOr(context, 0),
                compressionLevel.getValueOr(context, 0),
                binding.switchRemoveJunkData.isChecked
            ) { text: String?, completion: Float ->
                requireActivity().runOnUiThread {
                    progressDialog.setMessage(text)
                    dialogProgressBinding.updateProgress.progress =
                        (completion * PROGRESS_RESOLUTION).toInt()
                }
                !canceled
            }

            if (!canceled) {
                requireActivity().runOnUiThread {
                    progressDialog.dismiss()

                    val builder = MaterialAlertDialogBuilder(context)
                    if (success) {
                        builder.setMessage(R.string.convert_success_message)
                            .setCancelable(false)
                            .setPositiveButton(R.string.ok) { dialog: DialogInterface, _: Int ->
                                dialog.dismiss()
                                requireActivity().finish()
                            }
                    } else {
                        builder.setMessage(R.string.convert_failure_message)
                            .setPositiveButton(R.string.ok) { dialog: DialogInterface, _: Int -> dialog.dismiss() }
                    }
                    builder.show()
                }
            }
        }
        thread!!.start()
    }

    private fun joinThread() {
        if (thread != null) {
            try {
                thread!!.join()
            } catch (ignored: InterruptedException) {
            }
        }
    }

    companion object {
        private const val ARG_GAME_PATH = "game_path"

        private const val KEY_FORMAT = "convert_format"
        private const val KEY_BLOCK_SIZE = "convert_block_size"
        private const val KEY_COMPRESSION = "convert_compression"
        private const val KEY_COMPRESSION_LEVEL = "convert_compression_level"
        private const val KEY_REMOVE_JUNK_DATA = "remove_junk_data"

        private const val REQUEST_CODE_SAVE_FILE = 0

        private const val BLOB_TYPE_ISO = 0
        private const val BLOB_TYPE_GCZ = 3
        private const val BLOB_TYPE_WIA = 7
        private const val BLOB_TYPE_RVZ = 8

        private const val COMPRESSION_NONE = 0
        private const val COMPRESSION_PURGE = 1
        private const val COMPRESSION_BZIP2 = 2
        private const val COMPRESSION_LZMA = 3
        private const val COMPRESSION_LZMA2 = 4
        private const val COMPRESSION_ZSTD = 5

        private const val PROGRESS_RESOLUTION = 1000

        @JvmStatic
        fun newInstance(gamePath: String): ConvertFragment {
            val args = Bundle()
            args.putString(ARG_GAME_PATH, gamePath)
            val fragment = ConvertFragment()
            fragment.arguments = args
            return fragment
        }
    }
}
