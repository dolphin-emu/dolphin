// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.dialogs

import android.app.Dialog
import android.graphics.Bitmap
import android.os.Bundle
import android.view.View
import android.widget.ImageView
import org.dolphinemu.dolphinemu.services.GameFileCacheManager
import org.dolphinemu.dolphinemu.R
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import coil.imageLoader
import coil.request.ImageRequest
import kotlinx.coroutines.launch
import org.dolphinemu.dolphinemu.NativeLibrary
import org.dolphinemu.dolphinemu.databinding.DialogGameDetailsBinding
import org.dolphinemu.dolphinemu.databinding.DialogGameDetailsTvBinding
import org.dolphinemu.dolphinemu.model.GameFile

class GameDetailsDialog : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val gameFile = GameFileCacheManager.addOrGet(requireArguments().getString(ARG_GAME_PATH))

        val country = resources.getStringArray(R.array.countryNames)[gameFile.country]
        val fileSize = NativeLibrary.FormatSize(gameFile.fileSize, 2)

        // TODO: Remove dialog_game_details_tv if we switch to an AppCompatActivity for leanback
        val binding: DialogGameDetailsBinding
        val tvBinding: DialogGameDetailsTvBinding
        val builder = MaterialAlertDialogBuilder(requireContext())
        if (requireActivity() is AppCompatActivity) {
            binding = DialogGameDetailsBinding.inflate(layoutInflater)
            binding.apply {
                textGameTitle.text = gameFile.title
                textDescription.text = gameFile.description
                if (gameFile.description.isEmpty()) {
                    textDescription.visibility = View.GONE
                }

                textCountry.text = country
                textCompany.text = gameFile.company
                textGameId.text = gameFile.gameId
                textRevision.text = gameFile.revision.toString()

                if (!gameFile.shouldShowFileFormatDetails()) {
                    labelFileFormat.setText(R.string.game_details_file_size)
                    textFileFormat.text = fileSize

                    labelCompression.visibility = View.GONE
                    textCompression.visibility = View.GONE
                    labelBlockSize.visibility = View.GONE
                    textBlockSize.visibility = View.GONE
                } else {
                    val blockSize = gameFile.blockSize
                    val compression = gameFile.compressionMethod

                    textFileFormat.text = resources.getString(
                        R.string.game_details_size_and_format,
                        gameFile.fileFormatName,
                        fileSize
                    )

                    if (compression.isEmpty()) {
                        textCompression.setText(R.string.game_details_no_compression)
                    } else {
                        textCompression.text = gameFile.compressionMethod
                    }

                    if (blockSize > 0) {
                        textBlockSize.text = NativeLibrary.FormatSize(blockSize, 0)
                    } else {
                        labelBlockSize.visibility = View.GONE
                        textBlockSize.visibility = View.GONE
                    }
                }
            }

            this.lifecycleScope.launch {
                loadGameBanner(binding.banner, gameFile)
            }

            builder.setView(binding.root)
        } else {
            tvBinding = DialogGameDetailsTvBinding.inflate(layoutInflater)
            tvBinding.apply {
                textGameTitle.text = gameFile.title
                textDescription.text = gameFile.description
                if (gameFile.description.isEmpty()) {
                    tvBinding.textDescription.visibility = View.GONE
                }

                textCountry.text = country
                textCompany.text = gameFile.company
                textGameId.text = gameFile.gameId
                textRevision.text = gameFile.revision.toString()

                if (!gameFile.shouldShowFileFormatDetails()) {
                    labelFileFormat.setText(R.string.game_details_file_size)
                    textFileFormat.text = fileSize

                    labelCompression.visibility = View.GONE
                    textCompression.visibility = View.GONE
                    labelBlockSize.visibility = View.GONE
                    textBlockSize.visibility = View.GONE
                } else {
                    val blockSize = gameFile.blockSize
                    val compression = gameFile.compressionMethod

                    textFileFormat.text = resources.getString(
                        R.string.game_details_size_and_format,
                        gameFile.fileFormatName,
                        fileSize
                    )

                    if (compression.isEmpty()) {
                        textCompression.setText(R.string.game_details_no_compression)
                    } else {
                        textCompression.text = gameFile.compressionMethod
                    }

                    if (blockSize > 0) {
                        textBlockSize.text = NativeLibrary.FormatSize(blockSize, 0)
                    } else {
                        labelBlockSize.visibility = View.GONE
                        textBlockSize.visibility = View.GONE
                    }
                }
            }

            this.lifecycleScope.launch {
                loadGameBanner(tvBinding.banner, gameFile)
            }

            builder.setView(tvBinding.root)
        }
        return builder.create()
    }

    private suspend fun loadGameBanner(imageView: ImageView, gameFile: GameFile) {
        val vector = gameFile.banner
        val width = gameFile.bannerWidth
        val height = gameFile.bannerHeight

        imageView.scaleType = ImageView.ScaleType.FIT_CENTER
        val request = ImageRequest.Builder(imageView.context)
            .target(imageView)
            .error(R.drawable.no_banner)

        if (width > 0 && height > 0) {
            val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
            bitmap.setPixels(vector, 0, width, 0, 0, width, height)
            request.data(bitmap)
        } else {
            request.data(R.drawable.no_banner)
        }
        imageView.context.imageLoader.execute(request.build())
    }

    companion object {
        private const val ARG_GAME_PATH = "game_path"

        @JvmStatic
        fun newInstance(gamePath: String?): GameDetailsDialog {
            val fragment = GameDetailsDialog()
            val arguments = Bundle()
            arguments.putString(ARG_GAME_PATH, gamePath)
            fragment.arguments = arguments
            return fragment
        }
    }
}
