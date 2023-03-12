// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.input.ui;

import android.content.Context;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;

import androidx.appcompat.app.AlertDialog;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.google.android.material.divider.MaterialDividerItemDecoration;

import org.dolphinemu.dolphinemu.databinding.DialogAdvancedMappingBinding;
import org.dolphinemu.dolphinemu.features.input.model.ControllerInterface;
import org.dolphinemu.dolphinemu.features.input.model.CoreDevice;
import org.dolphinemu.dolphinemu.features.input.model.MappingCommon;
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.ControlReference;
import org.dolphinemu.dolphinemu.features.input.model.controlleremu.EmulatedController;

import java.util.Arrays;
import java.util.Optional;

public final class AdvancedMappingDialog extends AlertDialog
        implements AdapterView.OnItemClickListener
{
  private final DialogAdvancedMappingBinding mBinding;
  private final ControlReference mControlReference;
  private final EmulatedController mController;
  private final String[] mDevices;
  private final AdvancedMappingControlAdapter mControlAdapter;

  private String mSelectedDevice;

  public AdvancedMappingDialog(Context context, DialogAdvancedMappingBinding binding,
          ControlReference controlReference, EmulatedController controller)
  {
    super(context);

    mBinding = binding;
    mControlReference = controlReference;
    mController = controller;

    mDevices = ControllerInterface.getAllDeviceStrings();

    // TODO: Remove workaround for text filtering issue in material components when fixed
    // https://github.com/material-components/material-components-android/issues/1464
    mBinding.dropdownDevice.setSaveEnabled(false);

    binding.dropdownDevice.setOnItemClickListener(this);

    ArrayAdapter<String> deviceAdapter = new ArrayAdapter<>(
            context, android.R.layout.simple_spinner_dropdown_item, mDevices);
    binding.dropdownDevice.setAdapter(deviceAdapter);

    mControlAdapter = new AdvancedMappingControlAdapter(this::onControlClicked);
    mBinding.listControl.setAdapter(mControlAdapter);
    mBinding.listControl.setLayoutManager(new LinearLayoutManager(context));

    MaterialDividerItemDecoration divider =
            new MaterialDividerItemDecoration(context, LinearLayoutManager.VERTICAL);
    divider.setLastItemDecorated(false);
    mBinding.listControl.addItemDecoration(divider);

    binding.editExpression.setText(controlReference.getExpression());

    selectDefaultDevice();
  }

  public String getExpression()
  {
    return mBinding.editExpression.getText().toString();
  }

  @Override
  public void onItemClick(AdapterView<?> adapterView, View view, int position, long id)
  {
    setSelectedDevice(mDevices[position]);
  }

  private void setSelectedDevice(String deviceString)
  {
    mSelectedDevice = deviceString;

    CoreDevice device = ControllerInterface.getDevice(deviceString);
    if (device == null)
      setControls(new CoreDevice.Control[0]);
    else if (mControlReference.isInput())
      setControls(device.getInputs());
    else
      setControls(device.getOutputs());
  }

  private void setControls(CoreDevice.Control[] controls)
  {
    mControlAdapter.setControls(
            Arrays.stream(controls)
                    .map(CoreDevice.Control::getName)
                    .toArray(String[]::new));
  }

  private void onControlClicked(String control)
  {
    String expression = MappingCommon.getExpressionForControl(control, mSelectedDevice,
            mController.getDefaultDevice());

    int start = Math.max(mBinding.editExpression.getSelectionStart(), 0);
    int end = Math.max(mBinding.editExpression.getSelectionEnd(), 0);
    mBinding.editExpression.getText().replace(
            Math.min(start, end), Math.max(start, end), expression, 0, expression.length());
  }

  private void selectDefaultDevice()
  {
    String defaultDevice = mController.getDefaultDevice();
    boolean isInput = mControlReference.isInput();

    if (Arrays.asList(mDevices).contains(defaultDevice) &&
            (isInput || deviceHasOutputs(defaultDevice)))
    {
      // The default device is available, and it's an appropriate choice. Pick it
      setSelectedDevice(defaultDevice);
      mBinding.dropdownDevice.setText(defaultDevice, false);
      return;
    }
    else if (!isInput)
    {
      // Find the first device that has an output. (Most built-in devices don't have any)
      Optional<String> deviceWithOutputs = Arrays.stream(mDevices)
              .filter(AdvancedMappingDialog::deviceHasOutputs)
              .findFirst();

      if (deviceWithOutputs.isPresent())
      {
        setSelectedDevice(deviceWithOutputs.get());
        mBinding.dropdownDevice.setText(deviceWithOutputs.get(), false);
        return;
      }
    }

    // Nothing found
    setSelectedDevice("");
  }

  private static boolean deviceHasOutputs(String deviceString)
  {
    CoreDevice device = ControllerInterface.getDevice(deviceString);
    return device != null && device.getOutputs().length > 0;
  }
}
