libusb for Android
==================

Building:
---------

To build libusb for Android do the following:

 1. Download the latest NDK from:
    http://developer.android.com/tools/sdk/ndk/index.html

 2. Extract the NDK.

 3. Open a shell and make sure there exist an NDK global variable
    set to the directory where you extracted the NDK.

 4. Change directory to libusb's "android/jni"

 5. Run "$NDK/ndk-build".

The libusb library, examples and tests can then be found in:
    "android/libs/$ARCH"

Where $ARCH is one of:
    armeabi
    armeabi-v7a
    x86


Installing:
-----------

If you wish to use libusb from native code in own Android application
then you should add the following line to your Android.mk file:

  include $(PATH_TO_LIBUSB_SRC)/android/jni/libusb.mk

You will then need to add the following lines to the build
configuration for each native binary which uses libusb:

  LOCAL_C_INCLUDES += $(LIBUSB_ROOT_ABS)
  LOCAL_SHARED_LIBRARIES += libusb1.0

The Android build system will then correctly include libusb in the
application package (APK) file, provided ndk-build is invoked before
the package is built.


For a rooted device it is possible to install libusb into the system
image of a running device:

 1. Enable ADB on the device.

 2. Connect the device to a machine running ADB.

 3. Execute the following commands on the machine
    running ADB:

    # Make the system partition writable
    adb shell su -c "mount -o remount,rw /system"

    # Install libusb
    adb push obj/local/armeabi/libusb1.0.so /sdcard/
    adb shell su -c "cat > /system/lib/libusb1.0.so < /sdcard/libusb1.0.so"
    adb shell rm /system/lib/libusb1.0.so

    # Install the samples and tests
    for B in listdevs fxload xusb sam3u_benchmark hotplugtest stress
    do
      adb push "obj/local/armeabi/$B" /sdcard/
      adb shell su -c "cat > /system/bin/$B < /sdcard/$B"
      adb shell su -c "chmod 0755 /system/bin/$B"
      adb shell rm "/sdcard/$B"
    done

    # Make the system partition read only again
    adb shell su -c "mount -o remount,ro /system"

    # Run listdevs to
    adb shell su -c "listdevs"

 4. If your device only has a single OTG port then ADB can generally
    be switched to using Wifi with the following commands when connected
    via USB:

    adb shell netcfg
    # Note the wifi IP address of the phone
    adb tcpip 5555
    # Use the IP address from netcfg
    adb connect 192.168.1.123:5555

Runtime Permissions:
--------------------

The default system configuration on most Android device will not allow
access to USB devices. There are several options for changing this.

If you have control of the system image then you can modify the
ueventd.rc used in the image to change the permissions on
/dev/bus/usb/*/*. If using this approach then it is advisable to
create a new Android permission to protect access to these files.
It is not advisable to give all applications read and write permissions
to these files.

For rooted devices the code using libusb could be executed as root
using the "su" command. An alternative would be to use the "su" command
to change the permissions on the appropriate /dev/bus/usb/ files.

Users have reported success in using android.hardware.usb.UsbManager
to request permission to use the UsbDevice and then opening the
device. The difficulties in this method is that there is no guarantee
that it will continue to work in the future Android versions, it
requires invoking Java APIs and running code to match each
android.hardware.usb.UsbDevice to a libusb_device.
