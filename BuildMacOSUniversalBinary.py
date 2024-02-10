#!/usr/bin/env python3
"""
The current tooling supported in CMake, Homebrew, and Qt5 are insufficient for
creating macOS universal binaries automatically for applications like Dolphin
which have more complicated build requirements (like different libraries, build
flags and source files for each target architecture).

So instead, this script manages the configuration and compilation of distinct
builds and project files for each target architecture and then merges the two
binaries into a single universal binary.

Running this script will:
1) Generate Xcode project files for the ARM build (if project files don't
   already exist)
2) Generate Xcode project files for the x64 build (if project files don't
   already exist)
3) Build the ARM project for the selected build_target
4) Build the x64 project for the selected build_target
5) Generate universal .app packages combining the ARM and x64 packages
6) Use the lipo tool to combine the binary objects inside each of the
   packages into universal binaries
7) Code sign the final universal binaries using the specified
   codesign_identity
"""

import argparse
import filecmp
import glob
import json
import multiprocessing
import os
import shutil
import subprocess

# The config variables listed below are the defaults, but they can be
# overridden by command line arguments see parse_args(), or run:
# BuildMacOSUniversalBinary.py --help
DEFAULT_CONFIG = {

    # Location of destination universal binary
    "dst_app": "universal/",
    # Build Target (dolphin-emu to just build the emulator and skip the tests)
    "build_target": "ALL_BUILD",

    # Location for CMake to search for files (default is for homebrew)
    "arm64_cmake_prefix":  "/opt/homebrew",
    "x86_64_cmake_prefix": "/usr/local",

    # Locations to qt5 directories for arm and x64 libraries
    # The default values of these paths are taken from the default
    # paths used for homebrew
    "arm64_qt5_path":  "/opt/homebrew/opt/qt5",
    "x86_64_qt5_path": "/usr/local/opt/qt5",

    # Identity to use for code signing. "-" indicates that the app will not
    # be cryptographically signed/notarized but will instead just use a
    # SHA checksum to verify the integrity of the app. This doesn't
    # protect against malicious actors, but it does protect against
    # running corrupted binaries and allows for access to the extended
    # permisions needed for ARM builds
    "codesign_identity":  "-",
    # Entitlements file to use for code signing
    "entitlements": "../Source/Core/DolphinQt/DolphinEmu.entitlements",

    # Minimum macOS version for each architecture slice
    "arm64_mac_os_deployment_target":  "11.0.0",
    "x86_64_mac_os_deployment_target": "10.15.0",

    # CMake Generator to use for building
    "generator": "Unix Makefiles",
    "build_type": "Release",

    "run_unit_tests": False,

    # Whether we should make a build for Steam.
    "steam": False,

    # Whether our autoupdate functionality is enabled or not.
    "autoupdate": True,

    # The distributor for this build.
    "distributor": "None"
}

# Architectures to build for. This is explicity left out of the command line
# config options for several reasons:
# 1) Adding new architectures will generally require more code changes
# 2) Single architecture builds should utilize the normal generated cmake
#    project files rather than this wrapper script

ARCHITECTURES = ["x86_64", "arm64"]


def parse_args(conf=DEFAULT_CONFIG):
    """
    Parses the command line arguments into a config dictionary.
    """

    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument(
        "--target",
        help="Build target in generated project files",
        default=conf["build_target"],
        dest="build_target")
    parser.add_argument(
        "-G",
        help="CMake Generator to use for creating project files",
        default=conf["generator"],
        dest="generator")
    parser.add_argument(
        "--build_type",
        help="CMake build type [Debug, Release, RelWithDebInfo, MinSizeRel]",
        default=conf["build_type"],
        dest="build_type")
    parser.add_argument(
        "--dst_app",
        help="Directory where universal binary will be stored",
        default=conf["dst_app"])

    parser.add_argument(
        "--entitlements",
        help="Path to .entitlements file for code signing",
        default=conf["entitlements"])

    parser.add_argument("--run_unit_tests", action="store_true",
                        default=conf["run_unit_tests"])

    parser.add_argument(
        "--steam",
        help="Create a build for Steam",
        action="store_true",
        default=conf["steam"])

    parser.add_argument(
        "--autoupdate",
        help="Enables our autoupdate functionality",
        action=argparse.BooleanOptionalAction,
        default=conf["autoupdate"])

    parser.add_argument(
        "--distributor",
        help="Sets the distributor for this build",
        default=conf["distributor"])

    parser.add_argument(
        "--codesign",
        help="Code signing identity to use to sign the applications",
        default=conf["codesign_identity"],
        dest="codesign_identity")

    for arch in ARCHITECTURES:
        parser.add_argument(
             f"--{arch}_cmake_prefix",
             help="Folder for cmake to search for packages",
             default=conf[arch+"_cmake_prefix"],
             dest=arch+"_cmake_prefix")

        parser.add_argument(
             f"--{arch}_qt5_path",
             help=f"Install path for {arch} qt5 libraries",
             default=conf[arch+"_qt5_path"])

        parser.add_argument(
             f"--{arch}_mac_os_deployment_target",
             help=f"Deployment architecture for {arch} slice",
             default=conf[arch+"_mac_os_deployment_target"])

    return vars(parser.parse_args())


def lipo(path0, path1, dst):
    if subprocess.call(["lipo", "-create", "-output", dst, path0, path1]) != 0:
        print(f"WARNING: {path0} and {path1} cannot be lipo'd")

        shutil.copy(path0, dst)


def recursive_merge_binaries(src0, src1, dst):
    """
    Merges two build trees together for different architectures into a single
    universal binary.

    The rules for merging are:

    1) Files that exist in either src tree are copied into the dst tree
    2) Files that exist in both trees and are identical are copied over
       unmodified
    3) Files that exist in both trees and are non-identical are lipo'd
    4) Symlinks are created in the destination tree to mirror the hierarchy in
       the source trees
    """

    # Check that all files present in the folder are of the same type and that
    # links link to the same relative location
    for newpath0 in glob.glob(src0+"/*"):
        filename = os.path.basename(newpath0)
        newpath1 = os.path.join(src1, filename)
        if not os.path.exists(newpath1):
            continue

        if os.path.islink(newpath0) and os.path.islink(newpath1):
            if os.path.relpath(newpath0, src0) == os.path.relpath(newpath1, src1):
                continue

        if os.path.isdir(newpath0) and os.path.isdir(newpath1):
            continue

        # isfile() can be true for links so check that both are not links
        # before checking if they are both files
        if (not os.path.islink(newpath0)) and (not os.path.islink(newpath1)):
            if os.path.isfile(newpath0) and os.path.isfile(newpath1):
                continue

        raise Exception(f"{newpath0} and {newpath1} cannot be " +
                        "merged into a universal binary because they are of " +
                        "incompatible types. Perhaps the installed libraries" +
                        " are from different versions for each architecture")

    for newpath0 in glob.glob(src0+"/*"):
        filename = os.path.basename(newpath0)
        newpath1 = os.path.join(src1, filename)
        new_dst_path = os.path.join(dst, filename)
        if os.path.islink(newpath0):
            # Symlinks will be fixed after files are resolved
            continue

        if not os.path.exists(newpath1):
            if os.path.isdir(newpath0):
                shutil.copytree(newpath0, new_dst_path)
            else:
                shutil.copy(newpath0, new_dst_path)

            continue

        if os.path.isdir(newpath1):
            os.mkdir(new_dst_path)
            recursive_merge_binaries(newpath0, newpath1, new_dst_path)
            continue

        if filecmp.cmp(newpath0, newpath1):
            shutil.copy(newpath0, new_dst_path)
        else:
            lipo(newpath0, newpath1, new_dst_path)

    # Loop over files in src1 and copy missing things over to dst
    for newpath1 in glob.glob(src1+"/*"):
        filename = os.path.basename(newpath1)
        newpath0 = os.path.join(src0, filename)
        new_dst_path = os.path.join(dst, filename)
        if (not os.path.exists(newpath0)) and (not os.path.islink(newpath1)):
            if os.path.isdir(newpath1):
                shutil.copytree(newpath1, new_dst_path)
            else:
                shutil.copy(newpath1, new_dst_path)

    # Fix up symlinks for path0
    for newpath0 in glob.glob(src0+"/*"):
        filename = os.path.basename(newpath0)
        new_dst_path = os.path.join(dst, filename)
        if os.path.islink(newpath0):
            relative_path = os.path.relpath(os.path.realpath(newpath0), src0)
            os.symlink(relative_path, new_dst_path)
    # Fix up symlinks for path1
    for newpath1 in glob.glob(src1+"/*"):
        filename = os.path.basename(newpath1)
        new_dst_path = os.path.join(dst, filename)
        newpath0 = os.path.join(src0, filename)
        if os.path.islink(newpath1) and not os.path.exists(newpath0):
            relative_path = os.path.relpath(os.path.realpath(newpath1), src1)
            os.symlink(relative_path, new_dst_path)

def python_to_cmake_bool(boolean):
    return "ON" if boolean else "OFF"

def build(config):
    """
    Builds the project with the parameters specified in config.
    """

    print("Building config:")
    print(json.dumps(config, indent=4))

    # Configure and build single architecture builds for each architecture
    for arch in ARCHITECTURES:
        if not os.path.exists(arch):
            os.mkdir(arch)

        # Place Qt on the prefix path.
        prefix_path = config[arch+"_qt5_path"]+';'+config[arch+"_cmake_prefix"]

        env = os.environ.copy()
        env["CMAKE_OSX_ARCHITECTURES"] = arch
        env["CMAKE_PREFIX_PATH"] = prefix_path

        # Add the other architecture's prefix path to the ignore path so that
        # CMake doesn't try to pick up the wrong architecture's libraries when
        # cross compiling.
        ignore_path = ""
        for a in ARCHITECTURES:
            if a != arch:
                ignore_path = config[a+"_cmake_prefix"]

        subprocess.check_call([
                "cmake", "../../", "-G", config["generator"],
                "-DCMAKE_BUILD_TYPE=" + config["build_type"],
                '-DCMAKE_CXX_FLAGS="-DMACOS_UNIVERSAL_BUILD=1"',
                '-DCMAKE_C_FLAGS="-DMACOS_UNIVERSAL_BUILD=1"',
                # System name needs to be specified for CMake to use
                # the specified CMAKE_SYSTEM_PROCESSOR
                "-DCMAKE_SYSTEM_NAME=Darwin",
                "-DCMAKE_PREFIX_PATH="+prefix_path,
                "-DCMAKE_SYSTEM_PROCESSOR="+arch,
                "-DCMAKE_IGNORE_PATH="+ignore_path,
                "-DCMAKE_OSX_DEPLOYMENT_TARGET="
                + config[arch+"_mac_os_deployment_target"],
                "-DMACOS_CODE_SIGNING_IDENTITY="
                + config["codesign_identity"],
                "-DMACOS_CODE_SIGNING_IDENTITY_UPDATER="
                + config["codesign_identity"],
                '-DMACOS_CODE_SIGNING="ON"',
                "-DSTEAM="
                + python_to_cmake_bool(config["steam"]),
                "-DENABLE_AUTOUPDATE="
                + python_to_cmake_bool(config["autoupdate"]),
                '-DDISTRIBUTOR=' + config['distributor']
            ],
            env=env, cwd=arch)

        threads = multiprocessing.cpu_count()
        subprocess.check_call(["cmake", "--build", ".",
                               "--config", config["build_type"],
                               "--parallel", f"{threads}"], cwd=arch)

    dst_app = config["dst_app"]

    if os.path.exists(dst_app):
        shutil.rmtree(dst_app)

    # Create and codesign the universal binary/
    os.mkdir(dst_app)

    # Source binary trees to merge together
    src_app0 = ARCHITECTURES[0]+"/Binaries/"
    src_app1 = ARCHITECTURES[1]+"/Binaries/"

    recursive_merge_binaries(src_app0, src_app1, dst_app)
    for path in glob.glob(dst_app+"/*"):
        if os.path.isdir(path) and os.path.splitext(path)[1] != ".app":
            continue

        subprocess.check_call([
            "codesign",
            "-d",
            "--force",
            "-s",
            config["codesign_identity"],
            "--options=runtime",
            "--entitlements", config["entitlements"],
            "--deep",
            "--verbose=2",
            path])

    print("Built Universal Binary successfully!")

    # Build and run unit tests for each architecture
    unit_test_results = {}
    if config["run_unit_tests"]:
        for arch in ARCHITECTURES:
            if not os.path.exists(arch):
                os.mkdir(arch)

            print(f"Building and running unit tests for: {arch}")
            unit_test_results[arch] = \
                subprocess.call(["cmake", "--build", ".",
                                 "--config", config["build_type"],
                                 "--target", "unittests",
                                 "--parallel", f"{threads}"], cwd=arch)

        passed_unit_tests = True
        for a in unit_test_results:
            code = unit_test_results[a]
            passed = code == 0

            status_string = "PASSED"
            if not passed:
                passed_unit_tests = False
                status_string = f"FAILED ({code})"

            print(a + " Unit Tests: " + status_string)

        if not passed_unit_tests:
            exit(-1)

        print("Passed all unit tests")


if __name__ == "__main__":
    conf = parse_args()
    build(conf)
