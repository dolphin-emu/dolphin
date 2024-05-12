// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.settings.ui

import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.graphics.PorterDuff
import android.os.Build
import android.provider.DocumentsContract
import android.text.format.DateFormat
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView
import androidx.annotation.ColorInt
import androidx.appcompat.app.AlertDialog
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentActivity
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.color.MaterialColors
import com.google.android.material.datepicker.CalendarConstraints
import com.google.android.material.datepicker.MaterialDatePicker
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.elevation.ElevationOverlayProvider
import com.google.android.material.slider.Slider
import com.google.android.material.timepicker.MaterialTimePicker
import com.google.android.material.timepicker.TimeFormat
import org.dolphinemu.dolphinemu.R
import org.dolphinemu.dolphinemu.databinding.*
import org.dolphinemu.dolphinemu.features.input.model.view.InputMappingControlSetting
import org.dolphinemu.dolphinemu.features.input.ui.AdvancedMappingDialog
import org.dolphinemu.dolphinemu.features.input.ui.MotionAlertDialog
import org.dolphinemu.dolphinemu.features.input.ui.viewholder.InputMappingControlSettingViewHolder
import org.dolphinemu.dolphinemu.features.settings.model.Settings
import org.dolphinemu.dolphinemu.features.settings.model.view.*
import org.dolphinemu.dolphinemu.features.settings.ui.viewholder.*
import org.dolphinemu.dolphinemu.utils.DirectoryInitialization
import org.dolphinemu.dolphinemu.utils.FileBrowserHelper
import org.dolphinemu.dolphinemu.utils.Log
import org.dolphinemu.dolphinemu.utils.PermissionsHandler
import java.io.File
import java.io.IOException
import java.io.RandomAccessFile
import java.util.*
import kotlin.math.roundToInt

class SettingsAdapter(
    private val fragmentView: SettingsFragmentView,
    private val context: Context
) :
    RecyclerView.Adapter<SettingViewHolder>(), DialogInterface.OnClickListener,
    Slider.OnChangeListener {
    private var settingsList: ArrayList<SettingsItem>? = null
    private var clickedItem: SettingsItem? = null
    private var clickedPosition: Int = -1
    private var seekbarProgress: Float = 0f
    private var dialog: AlertDialog? = null
    private var textSliderValue: TextView? = null

    val settings: Settings?
        get() = fragmentView.settings

    val fragmentActivity: FragmentActivity
        get() = fragmentView.fragmentActivity

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SettingViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            SettingsItem.TYPE_HEADER -> HeaderViewHolder(
                ListItemHeaderBinding.inflate(inflater),
                this
            )
            SettingsItem.TYPE_SWITCH -> SwitchSettingViewHolder(
                ListItemSettingSwitchBinding.inflate(inflater),
                this
            )
            SettingsItem.TYPE_STRING_SINGLE_CHOICE,
            SettingsItem.TYPE_SINGLE_CHOICE_DYNAMIC_DESCRIPTIONS,
            SettingsItem.TYPE_SINGLE_CHOICE -> SingleChoiceViewHolder(
                ListItemSettingBinding.inflate(inflater),
                this
            )
            SettingsItem.TYPE_SLIDER -> SliderViewHolder(
                ListItemSettingBinding.inflate(
                    inflater
                ), this, context
            )
            SettingsItem.TYPE_SUBMENU -> SubmenuViewHolder(
                ListItemSubmenuBinding.inflate(
                    inflater
                ), this
            )
            SettingsItem.TYPE_INPUT_MAPPING_CONTROL -> InputMappingControlSettingViewHolder(
                ListItemMappingBinding.inflate(inflater),
                this
            )
            SettingsItem.TYPE_FILE_PICKER -> FilePickerViewHolder(
                ListItemSettingBinding.inflate(
                    inflater
                ), this
            )
            SettingsItem.TYPE_RUN_RUNNABLE -> RunRunnableViewHolder(
                ListItemSettingBinding.inflate(
                    inflater
                ), this, context
            )
            SettingsItem.TYPE_STRING -> InputStringSettingViewHolder(
                ListItemSettingBinding.inflate(
                    inflater
                ), this
            )
            SettingsItem.TYPE_HYPERLINK_HEADER -> HeaderHyperLinkViewHolder(
                ListItemHeaderBinding.inflate(
                    inflater
                ), this
            )
            SettingsItem.TYPE_DATETIME_CHOICE -> DateTimeSettingViewHolder(
                ListItemSettingBinding.inflate(
                    inflater
                ), this
            )
            else -> throw IllegalArgumentException("Invalid view type: $viewType")
        }
    }

    override fun onBindViewHolder(holder: SettingViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    private fun getItem(position: Int): SettingsItem {
        return settingsList!![position]
    }

    override fun getItemCount(): Int {
        return if (settingsList != null) {
            settingsList!!.size
        } else {
            0
        }
    }

    override fun getItemViewType(position: Int): Int {
        return getItem(position).type
    }

    fun setSettings(settings: ArrayList<SettingsItem>) {
        settingsList = settings
        notifyDataSetChanged()
    }

    fun clearSetting(item: SettingsItem) {
        item.clear(settings!!)
        fragmentView.onSettingChanged()
    }

    fun notifyAllSettingsChanged() {
        notifyItemRangeChanged(0, itemCount)
        fragmentView.onSettingChanged()
    }

    fun onBooleanClick(item: SwitchSetting, checked: Boolean) {
        item.setChecked(settings!!, checked)
        fragmentView.onSettingChanged()
    }

    fun onInputStringClick(item: InputStringSetting, position: Int) {
        val inflater = LayoutInflater.from(context)
        val binding = DialogInputStringBinding.inflate(inflater)
        val input = binding.input
        input.setText(item.selectedValue)
        dialog = MaterialAlertDialogBuilder(fragmentView.fragmentActivity)
            .setView(binding.root)
            .setMessage(item.description)
            .setPositiveButton(R.string.ok) { _: DialogInterface?, _: Int ->
                val editTextInput = input.text.toString()
                if (item.selectedValue != editTextInput) {
                    notifyItemChanged(position)
                    fragmentView.onSettingChanged()
                }
                item.setSelectedValue(fragmentView.settings!!, editTextInput)
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    fun onSingleChoiceClick(item: SingleChoiceSetting, position: Int) {
        clickedItem = item
        clickedPosition = position
        val value = getSelectionForSingleChoiceValue(item)
        dialog = MaterialAlertDialogBuilder(fragmentView.fragmentActivity)
            .setTitle(item.name)
            .setSingleChoiceItems(item.choicesId, value, this)
            .show()
    }

    fun onStringSingleChoiceClick(item: StringSingleChoiceSetting, position: Int) {
        clickedItem = item
        clickedPosition = position
        item.refreshChoicesAndValues()
        val choices = item.choices
        val noChoicesAvailableString = item.noChoicesAvailableString
        dialog = if (noChoicesAvailableString != 0 && choices.isEmpty()) {
            MaterialAlertDialogBuilder(fragmentView.fragmentActivity)
                .setTitle(item.name)
                .setMessage(noChoicesAvailableString)
                .setPositiveButton(R.string.ok, null)
                .show()
        } else {
            MaterialAlertDialogBuilder(fragmentView.fragmentActivity)
                .setTitle(item.name)
                .setSingleChoiceItems(
                    item.choices, item.selectedValueIndex,
                    this
                )
                .show()
        }
    }

    fun onSingleChoiceDynamicDescriptionsClick(
        item: SingleChoiceSettingDynamicDescriptions,
        position: Int
    ) {
        clickedItem = item
        clickedPosition = position

        val value = getSelectionForSingleChoiceDynamicDescriptionsValue(item)

        dialog = MaterialAlertDialogBuilder(fragmentView.fragmentActivity)
            .setTitle(item.name)
            .setSingleChoiceItems(item.choicesId, value, this)
            .show()
    }

    fun onSliderClick(item: SliderSetting, position: Int) {
        clickedItem = item
        clickedPosition = position
        seekbarProgress = when (item) {
            is FloatSliderSetting -> item.selectedValue
            is IntSliderSetting -> item.selectedValue.toFloat()
        }

        val inflater = LayoutInflater.from(fragmentView.fragmentActivity)
        val binding = DialogSliderBinding.inflate(inflater)

        textSliderValue = binding.textValue
        textSliderValue!!.text = if (item.showDecimal) {
            String.format("%.2f", seekbarProgress)
        } else {
            seekbarProgress.toInt().toString()
        }

        binding.textUnits.text = item.units

        val slider = binding.slider
        when (item) {
            is FloatSliderSetting -> {
                slider.valueFrom = item.min
                slider.valueTo = item.max
                slider.stepSize = item.stepSize
            }
            is IntSliderSetting -> {
                slider.valueFrom = item.min.toFloat()
                slider.valueTo = item.max.toFloat()
                slider.stepSize = item.stepSize.toFloat()
            }
        }
        slider.value = (seekbarProgress / slider.stepSize).roundToInt() * slider.stepSize
        slider.addOnChangeListener(this)

        dialog = MaterialAlertDialogBuilder(fragmentView.fragmentActivity)
            .setTitle(item.name)
            .setView(binding.root)
            .setPositiveButton(R.string.ok, this)
            .show()
    }

    fun onSubmenuClick(item: SubmenuSetting) {
        fragmentView.loadSubMenu(item.menuKey)
    }

    fun onInputMappingClick(item: InputMappingControlSetting, position: Int) {
        if (item.controller.getDefaultDevice().isEmpty() && !fragmentView.isMappingAllDevices) {
            MaterialAlertDialogBuilder(fragmentView.fragmentActivity)
                .setMessage(R.string.input_binding_no_device)
                .setPositiveButton(R.string.ok, this)
                .show()
            return
        }

        val dialog = MotionAlertDialog(
            fragmentView.fragmentActivity, item,
            fragmentView.isMappingAllDevices
        )

        val background = ContextCompat.getDrawable(context, R.drawable.dialog_round)
        @ColorInt val color = ElevationOverlayProvider(dialog.context).compositeOverlay(
            MaterialColors.getColor(dialog.window!!.decorView, R.attr.colorSurface),
            dialog.window!!.decorView.elevation
        )
        background!!.setColorFilter(color, PorterDuff.Mode.SRC_ATOP)
        dialog.window!!.setBackgroundDrawable(background)

        dialog.setTitle(R.string.input_binding)
        dialog.setMessage(
            String.format(
                context.getString(R.string.input_binding_description),
                item.name
            )
        )
        dialog.setButton(AlertDialog.BUTTON_NEGATIVE, context.getString(R.string.cancel), this)
        dialog.setButton(
            AlertDialog.BUTTON_NEUTRAL,
            context.getString(R.string.clear)
        ) { _: DialogInterface?, _: Int -> item.clearValue() }
        dialog.setOnDismissListener {
            notifyItemChanged(position)
            fragmentView.onSettingChanged()
        }
        dialog.setCanceledOnTouchOutside(false)
        dialog.show()
    }

    fun onAdvancedInputMappingClick(item: InputMappingControlSetting, position: Int) {
        val inflater = LayoutInflater.from(context)
        val binding = DialogAdvancedMappingBinding.inflate(inflater)
        val dialog = AdvancedMappingDialog(
            context,
            binding,
            item.controlReference,
            item.controller
        )

        val background = ContextCompat.getDrawable(context, R.drawable.dialog_round)
        @ColorInt val color = ElevationOverlayProvider(dialog.context).compositeOverlay(
            MaterialColors.getColor(dialog.window!!.decorView, R.attr.colorSurface),
            dialog.window!!.decorView.elevation
        )
        background!!.setColorFilter(color, PorterDuff.Mode.SRC_ATOP)
        dialog.window!!.setBackgroundDrawable(background)

        dialog.setTitle(if (item.isInput) R.string.input_configure_input else R.string.input_configure_output)
        dialog.setView(binding.root)
        dialog.setButton(
            AlertDialog.BUTTON_POSITIVE, context.getString(R.string.ok)
        ) { _: DialogInterface?, _: Int ->
            item.value = dialog.expression
            notifyItemChanged(position)
            fragmentView.onSettingChanged()
        }
        dialog.setButton(AlertDialog.BUTTON_NEGATIVE, context.getString(R.string.cancel), this)
        dialog.setButton(
            AlertDialog.BUTTON_NEUTRAL,
            context.getString(R.string.clear)
        ) { _: DialogInterface?, _: Int ->
            item.clearValue()
            notifyItemChanged(position)
            fragmentView.onSettingChanged()
        }
        dialog.setCanceledOnTouchOutside(false)
        dialog.show()
    }

    fun onFilePickerDirectoryClick(item: SettingsItem, position: Int) {
        clickedItem = item
        clickedPosition = position

        if (!PermissionsHandler.isExternalStorageLegacy()) {
            MaterialAlertDialogBuilder(context)
                .setMessage(R.string.path_not_changeable_scoped_storage)
                .setPositiveButton(R.string.ok) { dialog: DialogInterface, _: Int -> dialog.dismiss() }
                .show()
        } else {
            FileBrowserHelper.openDirectoryPicker(
                fragmentView.fragmentActivity,
                FileBrowserHelper.GAME_EXTENSIONS
            )
        }
    }

    fun onFilePickerFileClick(item: SettingsItem, position: Int) {
        clickedItem = item
        clickedPosition = position
        val filePicker = item as FilePicker

        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT)
        intent.addCategory(Intent.CATEGORY_OPENABLE)
        intent.type = "*/*"
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, filePicker.getSelectedValue())
        }

        fragmentView.fragmentActivity.startActivityForResult(intent, filePicker.requestType)
    }

    fun onDateTimeClick(item: DateTimeChoiceSetting, position: Int) {
        clickedItem = item
        clickedPosition = position
        val storedTime = java.lang.Long.decode(item.getSelectedValue()) * 1000

        // Helper to extract hour and minute from epoch time
        val calendar = Calendar.getInstance()
        calendar.timeInMillis = storedTime
        calendar.timeZone = TimeZone.getTimeZone("UTC")

        // Start and end epoch times available for the Wii's date picker
        val calendarConstraints = CalendarConstraints.Builder()
            .setStart(946684800000L)
            .setEnd(2082672000000L)
            .build()

        var timeFormat = TimeFormat.CLOCK_12H
        if (DateFormat.is24HourFormat(fragmentView.fragmentActivity)) {
            timeFormat = TimeFormat.CLOCK_24H
        }

        val datePicker = MaterialDatePicker.Builder.datePicker()
            .setSelection(storedTime)
            .setTitleText(R.string.select_rtc_date)
            .setCalendarConstraints(calendarConstraints)
            .build()
        val timePicker = MaterialTimePicker.Builder()
            .setTimeFormat(timeFormat)
            .setHour(calendar[Calendar.HOUR_OF_DAY])
            .setMinute(calendar[Calendar.MINUTE])
            .setTitleText(R.string.select_rtc_time)
            .build()

        datePicker.addOnPositiveButtonClickListener {
            timePicker.show(
                fragmentView.fragmentActivity.supportFragmentManager,
                "TimePicker"
            )
        }
        timePicker.addOnPositiveButtonClickListener {
            var epochTime = datePicker.selection!! / 1000
            epochTime += timePicker.hour.toLong() * 60 * 60
            epochTime += timePicker.minute.toLong() * 60
            val rtcString = "0x" + java.lang.Long.toHexString(epochTime)
            if (item.getSelectedValue() != rtcString) {
                notifyItemChanged(clickedPosition)
                fragmentView.onSettingChanged()
            }
            item.setSelectedValue(fragmentView.settings!!, rtcString)
            clickedItem = null
        }
        datePicker.show(fragmentView.fragmentActivity.supportFragmentManager, "DatePicker")
    }

    fun onFilePickerConfirmation(selectedFile: String) {
        val filePicker = clickedItem as FilePicker

        if (filePicker.getSelectedValue() != selectedFile) {
            notifyItemChanged(clickedPosition)
            fragmentView.onSettingChanged()
        }

        filePicker.setSelectedValue(fragmentView.settings!!, selectedFile)

        clickedItem = null
    }

    fun onMenuTagAction(menuTag: MenuTag, value: Int) {
        fragmentView.onMenuTagAction(menuTag, value)
    }

    fun hasMenuTagActionForValue(menuTag: MenuTag, value: Int): Boolean {
        return fragmentView.hasMenuTagActionForValue(menuTag, value)
    }

    override fun onClick(dialog: DialogInterface, which: Int) {
        when (clickedItem) {
            is SingleChoiceSetting -> {
                val scSetting = clickedItem as SingleChoiceSetting

                val value = getValueForSingleChoiceSelection(scSetting, which)
                if (scSetting.selectedValue != value) fragmentView.onSettingChanged()

                scSetting.setSelectedValue(settings!!, value)

                closeDialog()
            }
            is SingleChoiceSettingDynamicDescriptions -> {
                val scSetting = clickedItem as SingleChoiceSettingDynamicDescriptions

                val value = getValueForSingleChoiceDynamicDescriptionsSelection(scSetting, which)
                if (scSetting.selectedValue != value) fragmentView.onSettingChanged()

                scSetting.setSelectedValue(settings!!, value)

                closeDialog()
            }
            is StringSingleChoiceSetting -> {
                val scSetting = clickedItem as StringSingleChoiceSetting

                val value = scSetting.getValueAt(which)
                if (scSetting.selectedValue != value) fragmentView.onSettingChanged()

                scSetting.setSelectedValue(settings!!, value)

                closeDialog()
            }
            is IntSliderSetting -> {
                val sliderSetting = clickedItem as IntSliderSetting
                if (sliderSetting.selectedValue != seekbarProgress.toInt()) {
                    fragmentView.onSettingChanged()
                }
                sliderSetting.setSelectedValue(settings!!, seekbarProgress.toInt())
                closeDialog()
            }
            is FloatSliderSetting -> {
                val sliderSetting = clickedItem as FloatSliderSetting

                if (sliderSetting.selectedValue != seekbarProgress) fragmentView.onSettingChanged()

                sliderSetting.setSelectedValue(settings!!, seekbarProgress)

                closeDialog()
            }
        }
        clickedItem = null
        seekbarProgress = -1f
    }

    fun closeDialog() {
        if (dialog != null) {
            if (clickedPosition != -1) {
                notifyItemChanged(clickedPosition)
                clickedPosition = -1
            }
            dialog!!.dismiss()
            dialog = null
        }
    }

    override fun onValueChange(slider: Slider, progress: Float, fromUser: Boolean) {
        seekbarProgress = progress
        textSliderValue!!.text = if ((clickedItem as SliderSetting).showDecimal) {
            String.format("%.2f", seekbarProgress)
        } else {
            seekbarProgress.toInt().toString()
        }
    }

    private fun getValueForSingleChoiceSelection(item: SingleChoiceSetting, which: Int): Int {
        val valuesId = item.valuesId

        return if (valuesId > 0) {
            val valuesArray = context.resources.getIntArray(valuesId)
            valuesArray[which]
        } else {
            which
        }
    }

    private fun getSelectionForSingleChoiceValue(item: SingleChoiceSetting): Int {
        val value = item.selectedValue
        val valuesId = item.valuesId

        if (valuesId > 0) {
            val valuesArray = context.resources.getIntArray(valuesId)
            for (index in valuesArray.indices) {
                val current = valuesArray[index]
                if (current == value) {
                    return index
                }
            }
        } else {
            return value
        }
        return -1
    }

    private fun getValueForSingleChoiceDynamicDescriptionsSelection(
        item: SingleChoiceSettingDynamicDescriptions,
        which: Int
    ): Int {
        val valuesId = item.valuesId
        return if (valuesId > 0) {
            val valuesArray = context.resources.getIntArray(valuesId)
            valuesArray[which]
        } else {
            which
        }
    }

    private fun getSelectionForSingleChoiceDynamicDescriptionsValue(
        item: SingleChoiceSettingDynamicDescriptions
    ): Int {
        val value = item.selectedValue
        val valuesId = item.valuesId
        if (valuesId > 0) {
            val valuesArray = context.resources.getIntArray(valuesId)
            for (index in valuesArray.indices) {
                val current = valuesArray[index]
                if (current == value) {
                    return index
                }
            }
        } else {
            return value
        }
        return -1
    }

    companion object {
        fun clearLog() {
            // Don't delete the log in case it is being monitored by another app.
            val log = File(DirectoryInitialization.getUserDirectory() + "/Logs/dolphin.log")
            try {
                val raf = RandomAccessFile(log, "rw")
                raf.setLength(0)
            } catch (e: IOException) {
                Log.error("[SettingsAdapter] Failed to clear log file: " + e.message)
            }
        }
    }
}
