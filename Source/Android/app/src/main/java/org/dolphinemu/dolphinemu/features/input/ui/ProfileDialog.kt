// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.divider.MaterialDividerItemDecoration
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.DialogInputProfilesBinding
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.utils.SerializableHelper.serializable

class ProfileDialog : BottomSheetDialogFragment() {
    private lateinit var presenter: ProfileDialogPresenter

    private var _binding: DialogInputProfilesBinding? = null
    private val binding get() = _binding!!

    override fun onCreate(savedInstanceState: Bundle?) {
        val menuTag = requireArguments().serializable<MenuTag>(KEY_MENU_TAG)!!

        presenter = ProfileDialogPresenter(this, menuTag)

        super.onCreate(savedInstanceState)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = DialogInputProfilesBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.profileList.adapter = ProfileAdapter(requireContext(), presenter)
        binding.profileList.layoutManager = LinearLayoutManager(context)
        val divider = MaterialDividerItemDecoration(requireActivity(), LinearLayoutManager.VERTICAL)
        divider.isLastItemDecorated = false
        binding.profileList.addItemDecoration(divider)

        // You can't expand a bottom sheet with a controller/remote/other non-touch devices
        val behavior: BottomSheetBehavior<View> = BottomSheetBehavior.from(view.parent as View)
        if (!resources.getBoolean(R.bool.hasTouch)) {
            behavior.state = BottomSheetBehavior.STATE_EXPANDED
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    companion object {
        private const val KEY_MENU_TAG = "menu_tag"

        @JvmStatic
        fun create(menuTag: MenuTag): ProfileDialog {
            val dialog = ProfileDialog()
            val args = Bundle()
            args.putSerializable(KEY_MENU_TAG, menuTag)
            dialog.arguments = args
            return dialog
        }
    }
}
