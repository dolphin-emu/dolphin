// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.fragments

import android.os.Bundle
import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import org.dolphinemu.dolphinemu.BuildConfig
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.DialogAboutBinding
import org.dolphinemu.dolphinemu.databinding.DialogAboutTvBinding

class AboutDialogFragment : BottomSheetDialogFragment() {
    private var _bindingMobile: DialogAboutBinding? = null
    private var _bindingTv: DialogAboutTvBinding? = null

    private val bindingMobile get() = _bindingMobile!!
    private val bindingTv get() = _bindingTv!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        if (activity is AppCompatActivity) {
            _bindingMobile = DialogAboutBinding.inflate(layoutInflater)
            return bindingMobile.root
        }
        _bindingTv = DialogAboutTvBinding.inflate(layoutInflater)
        return bindingTv.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        if (!resources.getBoolean(R.bool.hasTouch)) {
            BottomSheetBehavior.from<View>(view.parent as View).state =
                BottomSheetBehavior.STATE_EXPANDED
        }

        val wark = resources.getString(R.string.wark)
        val branch = String.format(
            resources.getString(R.string.about_branch),
            BuildConfig.BRANCH
        )
        val revision = String.format(
            resources.getString(R.string.about_revision),
            BuildConfig.GIT_HASH
        )
        if (activity is AppCompatActivity) {
            bindingMobile.dolphinLogo.setOnLongClickListener {
                Toast.makeText(requireContext(), wark, Toast.LENGTH_SHORT).show()
                true
            }

            bindingMobile.websiteText.movementMethod = LinkMovementMethod.getInstance()
            bindingMobile.githubText.movementMethod = LinkMovementMethod.getInstance()
            bindingMobile.supportText.movementMethod = LinkMovementMethod.getInstance()

            bindingMobile.versionText.text = BuildConfig.VERSION_NAME
            bindingMobile.branchText.text = branch
            bindingMobile.revisionText.text = revision
        } else {
            bindingTv.dolphinLogo.setOnLongClickListener {
                Toast.makeText(requireContext(), wark, Toast.LENGTH_SHORT).show()
                true
            }

            bindingTv.websiteText.movementMethod = LinkMovementMethod.getInstance()
            bindingTv.githubText.movementMethod = LinkMovementMethod.getInstance()
            bindingTv.supportText.movementMethod = LinkMovementMethod.getInstance()

            bindingTv.versionText.text = BuildConfig.VERSION_NAME
            bindingTv.branchText.text = branch
            bindingTv.revisionText.text = revision

            bindingTv.dolphinLogo.setImageDrawable(ContextCompat.getDrawable(requireContext(), R.drawable.ic_dolphin))
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _bindingMobile = null
    }

    companion object {
        const val TAG = "AboutDialogFragment"
    }
}
