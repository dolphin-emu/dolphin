#!/bin/bash
#
# Runs unit tests (must have been built beforehand) on an Android device connected via adb.
#
# The current working directory must contain test executables. Normally (for AArch64) the path to use
# looks like: Source/Android/app/.cxx/RelWithDebInfo/<random characters>/arm64-v8a/Binaries/Tests

DEVICE_DIR="/data/local/tmp/dolphin-emu-tests"

# Prevent MingW MSYS from being "smart" and turning the path above into a Windows-style path
export MSYS_NO_PATHCONV=1
export MSYS2_ARG_CONV_EXCL="*"

# Remove old copy of the Sys directory
adb shell rm -r "$DEVICE_DIR"

# Push new copy of the Sys directory
if [ -d "Sys" ]; then
    adb push "Sys" "$DEVICE_DIR/Sys"
fi

# Push and run executables
for path in *; do
    f=$(basename "$path")

    if [ -f "$path" ]; then
        adb push "$path" "$DEVICE_DIR/$f" && adb shell chmod 775 "$DEVICE_DIR/$f" && adb shell "$DEVICE_DIR/$f"
        RESULT=$(($RESULT+$?))

        # Some of these executables are pretty big, so let's remove them as soon as we're done
        adb shell rm "$DEVICE_DIR/$f"
    fi
done

echo ""
echo "Total failed tests: $RESULT"

exit $RESULT
