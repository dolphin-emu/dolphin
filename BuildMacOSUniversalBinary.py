#!/usr/bin/env python3
"""
The current tooling supported in CMake, Homebrew, and QT5 are insufficient for
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
5) Generates universal .app packages combining the ARM and x64 packages
6) Utilizes the lipo tool to combine the binary objects inside each of the
   packages into universal binaries
7) Code signs the final universal binaries using the specified
   codesign_identity
"""

import argparse
import filecmp
import glob
import json
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

    # Locations to pkg config files for arm and x64 libraries
    # The default values of these paths are taken from the default
    # paths used for homebrew
    "arm64_pkg_config_path":  '/opt/homebrew/lib/pkgconfig',
    "x86_64_pkg_config_path": '/usr/local/lib/pkgconfig',

    # Locations to qt5 directories for arm and x64 libraries
    # The default values of these paths are taken from the default
    # paths used for homebrew
    "arm64_qt5_path":  '/opt/homebrew/opt/qt5',
    "x86_64_qt5_path": '/usr/local/opt/qt5',

    # Identity to use for code signing. "-" indicates that the app will not
    # be cryptographically signed/notarized but will instead just use a
    # SHA checksum to verify the integrity of the app. This doesn't
    # protect against malicious actors, but it does protect against
    # running corrupted binaries and allows for access to the extended
    # permisions needed for ARM builds
    "codesign_identity":  '-',
    # Entitlements file to use for code signing
    "entitlements": "../Source/Core/DolphinQt/DolphinEmu.entitlements",

    # Minimum macOS version for each architecture slice
    "arm64_mac_os_deployment_target":  "11.0.0",
    "x86_64_mac_os_deployment_target": "10.12.0"

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
        '--target',
        help='Build target in generated project files',
        default=conf["build_target"],
        dest="build_target")

    parser.add_argument(
        '--dst_app',
        help='Directory where universal binary will be stored',
        default=conf["dst_app"])

    parser.add_argument(
        '--entitlements',
        help='Path to .entitlements file for code signing',
        default=conf["entitlements"])

    parser.add_argument(
        '--codesign',
        help='Code signing identity to use to sign the applications',
        default=conf["codesign_identity"],
        dest="codesign_identity")

    for arch in ARCHITECTURES:
        parser.add_argument(
             '--{}_pkg_config'.format(arch),
             help="Folder containing .pc files for {} libraries".format(arch),
             default=conf[arch+"_pkg_config_path"],
             dest=arch+"_pkg_config_path")

        parser.add_argument(
             '--{}_qt5_path'.format(arch),
             help="Install path for {} qt5 libraries".format(arch),
             default=conf[arch+"_qt5_path"])

        parser.add_argument(
             '--{}_mac_os_deployment_target'.format(arch),
             help="Deployment architecture for {} slice".format(arch),
             default=conf[arch+"_mac_os_deployment_target"])

    return vars(parser.parse_args())


def lipo(path0, path1, dst):
    if subprocess.call(['lipo', '-create', '-output', dst, path0, path1]) != 0:
        print("WARNING: {0} and {1} can not be lipo'd, keeping {0}"
              .format(path0, path1))

        shutil.copy(path0, dst)


def recursiveMergeBinaries(src0, src1, dst):
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

    for newpath0 in glob.glob(src0+"/*"):
        filename = os.path.basename(newpath0)
        newpath1 = os.path.join(src1, filename)
        new_dst_path = os.path.join(dst, filename)
        if os.path.islink(newpath0):
            # Symlinks will be fixed after files are resolved
            continue

        if not os.path.exists(newpath1):
            shutil.copy(newpath0, new_dst_path)
            continue

        if os.path.isdir(newpath1):
            os.mkdir(new_dst_path)
            recursiveMergeBinaries(newpath0, newpath1, new_dst_path)
            continue

        if filecmp.cmp(newpath0, newpath1):
            shutil.copy(newpath0, new_dst_path)
        else:
            lipo(newpath0, newpath1, new_dst_path)

    # Loop over files in src1 and copy missing things over to dst
    for newpath1 in glob.glob(src1+"/*"):
        filename = os.path.basename(newpath0)
        newpath0 = os.path.join(src0, filename)
        new_dst_path = os.path.join(dst, filename)
        if not os.path.exists(newpath0) and not os.path.islink(newpath1):
            shutil.copytree(newpath1, new_dst_path)

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

        env = os.environ.copy()
        env['PKG_CONFIG_PATH'] = config[arch+"_pkg_config_path"]
        env['Qt5_DIR'] = config[arch+"_qt5_path"]
        env['CMAKE_OSX_ARCHITECTURES'] = arch

        subprocess.check_call([
                'arch', '-'+arch,
                'cmake', '../../', '-G', 'Xcode',
                '-DCMAKE_OSX_DEPLOYMENT_TARGET='
                + config[arch+"_mac_os_deployment_target"],
                '-DMACOS_CODE_SIGNING_IDENTITY='
                + config['codesign_identity'],
                '-DMACOS_CODE_SIGNING_IDENTITY_UPDATER='
                + config['codesign_identity'],
                '-DMACOS_CODE_SIGNING="ON"'
            ],
            env=env, cwd=arch)

        # Build project
        subprocess.check_call(['xcodebuild',
                               '-project', 'dolphin-emu.xcodeproj',
                               '-target', config["build_target"],
                               '-configuration', 'Release'], cwd=arch)

    dst_app = config["dst_app"]

    if os.path.exists(dst_app):
        shutil.rmtree(dst_app)

    # Create and codesign the universal binary/
    os.mkdir(dst_app)

    # Source binary trees to merge together
    src_app0 = ARCHITECTURES[0]+"/Binaries/release"
    src_app1 = ARCHITECTURES[1]+"/Binaries/release"

    recursiveMergeBinaries(src_app0, src_app1, dst_app)
    for path in glob.glob(dst_app+"/*"):
        subprocess.check_call([
            'codesign',
            '-d',
            '--force',
            '-s',
            config["codesign_identity"],
            '--options=runtime',
            '--entitlements', config["entitlements"],
            '--deep',
            '--verbose=2',
            path])


if __name__ == "__main__":
    conf = parse_args()
    build(conf)
    print("Built Universal Binary successfully!")
