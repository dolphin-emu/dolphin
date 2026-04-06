function(dolphin_inject_version_info target)
  set(INFO_PLIST_PATH "$<TARGET_BUNDLE_DIR:${target}>/Contents/Info.plist")
  add_custom_command(TARGET ${target}
    POST_BUILD

    COMMAND /usr/libexec/PlistBuddy -c
    "Delete :CFBundleShortVersionString"
    "${INFO_PLIST_PATH}"
    || true

    COMMAND /usr/libexec/PlistBuddy -c
    "Delete :CFBundleLongVersionString"
    "${INFO_PLIST_PATH}"
    || true

    COMMAND /usr/libexec/PlistBuddy -c
    "Delete :CFBundleVersion"
    "${INFO_PLIST_PATH}"
    || true

    COMMAND /usr/libexec/PlistBuddy -c
    "Merge '${CMAKE_BINARY_DIR}/Source/Core/VersionInfo.plist'"
    "${INFO_PLIST_PATH}")
endfunction()
