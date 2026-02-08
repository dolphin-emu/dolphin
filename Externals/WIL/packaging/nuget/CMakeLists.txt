
file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/build_tools/nuget.exe" nuget_exe)
file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}" nupkg_dir)
file(TO_NATIVE_PATH "${nupkg_dir}/Microsoft.Windows.ImplementationLibrary.${WIL_BUILD_VERSION}.nupkg" wil_nupkg)

# The build servers don't have an up-to-date version of nuget, so pull it down ourselves...
file(DOWNLOAD https://dist.nuget.org/win-x86-commandline/latest/nuget.exe ${nuget_exe})

file(GLOB_RECURSE wil_headers ${CMAKE_SOURCE_DIR}/include/*.h)
file(GLOB_RECURSE wil_natvis ${CMAKE_SOURCE_DIR}/natvis/*.natvis)

add_custom_command(OUTPUT ${wil_nupkg}
    COMMAND ${nuget_exe} pack ${CMAKE_CURRENT_SOURCE_DIR}/Microsoft.Windows.ImplementationLibrary.nuspec -OutputDirectory ${nupkg_dir} -Version ${WIL_BUILD_VERSION} -NonInteractive
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/Microsoft.Windows.ImplementationLibrary.nuspec
        ${CMAKE_CURRENT_SOURCE_DIR}/Microsoft.Windows.ImplementationLibrary.targets
        ${wil_headers}
        ${wil_natvis}
        ${CMAKE_SOURCE_DIR}/LICENSE
        ${CMAKE_SOURCE_DIR}/ThirdPartyNotices.txt)

add_custom_target(make_wil_nupkg DEPENDS ${wil_nupkg})
