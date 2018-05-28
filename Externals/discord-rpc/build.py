#!/usr/bin/env python

import os
import subprocess
import sys
import shutil
import zipfile
from contextlib import contextmanager
import click


def get_platform():
    """ a name for the platform """
    if sys.platform.startswith('win'):
        return 'win'
    elif sys.platform == 'darwin':
        return 'osx'
    elif sys.platform.startswith('linux'):
        return 'linux'
    raise Exception('Unsupported platform ' + sys.platform)


SCRIPT_PATH = os.path.dirname(os.path.abspath(__file__))
# we use Buildkite which sets this env variable by default
IS_BUILD_MACHINE = os.environ.get('CI', '') == 'true'
PLATFORM = get_platform()
INSTALL_ROOT = os.path.join(SCRIPT_PATH, 'builds', 'install')


def get_signtool():
    """ get path to code signing tool """
    if PLATFORM == 'win':
        sdk_dir = 'c:\\Program Files (x86)\\Windows Kits\\10'  # os.environ['WindowsSdkDir']
        return os.path.join(sdk_dir, 'bin', 'x86', 'signtool.exe')
    elif PLATFORM == 'osx':
        return '/usr/bin/codesign'


@contextmanager
def cd(new_dir):
    """ Temporarily change current directory """
    if new_dir:
        old_dir = os.getcwd()
        os.chdir(new_dir)
    yield
    if new_dir:
        os.chdir(old_dir)


def mkdir_p(path):
    """ mkdir -p """
    if not os.path.isdir(path):
        click.secho('Making ' + path, fg='yellow')
        os.makedirs(path)


@click.group(invoke_without_command=True)
@click.pass_context
@click.option('--clean', is_flag=True)
def cli(ctx, clean):
    """ click wrapper for command line stuff """
    if ctx.invoked_subcommand is None:
        ctx.invoke(libs, clean=clean)
        if IS_BUILD_MACHINE:
            ctx.invoke(sign)
        ctx.invoke(archive)


@cli.command()
@click.pass_context
def unity(ctx):
    """ build just dynamic libs for use in unity project """
    ctx.invoke(libs, clean=False, static=False, shared=True, skip_formatter=True, just_release=True)
    BUILDS = []

    click.echo('--- Copying libs and header into unity example')
    UNITY_PROJECT_PATH = os.path.join(SCRIPT_PATH, 'examples', 'button-clicker', 'Assets', 'Plugins')

    if sys.platform.startswith('win'):
        LIBRARY_NAME = 'discord-rpc.dll'
        BUILD_64_BASE_PATH = os.path.join(SCRIPT_PATH, 'builds', 'win64-dynamic', 'src', 'Release')
        UNITY_64_DLL_PATH = os.path.join(UNITY_PROJECT_PATH, 'x86_64')
        BUILDS.append({BUILD_64_BASE_PATH: UNITY_64_DLL_PATH})

        BUILD_32_BASE_PATH = os.path.join(SCRIPT_PATH, 'builds', 'win32-dynamic', 'src', 'Release')
        UNITY_32_DLL_PATH = os.path.join(UNITY_PROJECT_PATH, 'x86')
        BUILDS.append({BUILD_32_BASE_PATH: UNITY_32_DLL_PATH})

    elif sys.platform == 'darwin':
        LIBRARY_NAME = 'discord-rpc.bundle'
        BUILD_BASE_PATH = os.path.join(SCRIPT_PATH, 'builds', 'osx-dynamic', 'src')
        UNITY_DLL_PATH = UNITY_PROJECT_PATH
        os.rename(
            os.path.join(BUILD_BASE_PATH, 'libdiscord-rpc.dylib'), os.path.join(BUILD_BASE_PATH, 'discord-rpc.bundle'))

        BUILDS.append({BUILD_BASE_PATH: UNITY_DLL_PATH})

    elif sys.platform.startswith('linux'):
        LIBRARY_NAME = 'discord-rpc.so'
        BUILD_BASE_PATH = os.path.join(SCRIPT_PATH, 'builds', 'linux-dynamic', 'src')
        UNITY_DLL_PATH = os.path.join(UNITY_PROJECT_PATH, 'x86')
        os.rename(os.path.join(BUILD_BASE_PATH, 'libdiscord-rpc.so'), os.path.join(BUILD_BASE_PATH, 'discord-rpc.so'))

        BUILDS.append({BUILD_BASE_PATH: UNITY_DLL_PATH})

    else:
        raise Exception('Unsupported platform ' + sys.platform)

    for build in BUILDS:
        for i in build:
            mkdir_p(build[i])
            shutil.copy(os.path.join(i, LIBRARY_NAME), build[i])


@cli.command()
@click.pass_context
def unreal(ctx):
    """ build libs and copy them into the unreal project """
    ctx.invoke(libs, clean=False, static=False, shared=True, skip_formatter=True, just_release=True)
    BUILDS = []

    click.echo('--- Copying libs and header into unreal example')
    UNREAL_PROJECT_PATH = os.path.join(SCRIPT_PATH, 'examples', 'unrealstatus', 'Plugins', 'discordrpc')
    UNREAL_INCLUDE_PATH = os.path.join(UNREAL_PROJECT_PATH, 'Source', 'ThirdParty', 'DiscordRpcLibrary', 'Include')
    mkdir_p(UNREAL_INCLUDE_PATH)
    shutil.copy(os.path.join(SCRIPT_PATH, 'include', 'discord_rpc.h'), UNREAL_INCLUDE_PATH)

    if sys.platform.startswith('win'):
        LIBRARY_NAME = 'discord-rpc.lib'
        BUILD_64_BASE_PATH = os.path.join(SCRIPT_PATH, 'builds', 'win64-dynamic', 'src', 'Release')
        UNREAL_64_DLL_PATH = os.path.join(UNREAL_PROJECT_PATH, 'Source', 'ThirdParty', 'DiscordRpcLibrary', 'Win64')
        BUILDS.append({BUILD_64_BASE_PATH: UNREAL_64_DLL_PATH})

        BUILD_32_BASE_PATH = os.path.join(SCRIPT_PATH, 'builds', 'win32-dynamic', 'src', 'Release')
        UNREAL_32_DLL_PATH = os.path.join(UNREAL_PROJECT_PATH, 'Source', 'ThirdParty', 'DiscordRpcLibrary', 'Win32')
        BUILDS.append({BUILD_32_BASE_PATH: UNREAL_32_DLL_PATH})

    elif sys.platform == 'darwin':
        LIBRARY_NAME = 'libdiscord-rpc.dylib'
        BUILD_BASE_PATH = os.path.join(SCRIPT_PATH, 'builds', 'osx-dynamic', 'src')
        UNREAL_DLL_PATH = os.path.join(UNREAL_PROJECT_PATH, 'Source', 'ThirdParty', 'DiscordRpcLibrary', 'Mac')

        BUILDS.append({BUILD_BASE_PATH: UNREAL_DLL_PATH})

    elif sys.platform.startswith('linux'):
        LIBRARY_NAME = 'libdiscord-rpc.so'
        BUILD_BASE_PATH = os.path.join(SCRIPT_PATH, 'builds', 'linux-dynamic', 'src')
        UNREAL_DLL_PATH = os.path.join(UNREAL_PROJECT_PATH, 'Source', 'ThirdParty', 'DiscordRpcLibrary', 'Linux')

        BUILDS.append({BUILD_BASE_PATH: UNREAL_DLL_PATH})

    else:
        raise Exception('Unsupported platform ' + sys.platform)

    for build in BUILDS:
        for i in build:
            mkdir_p(build[i])
            shutil.copy(os.path.join(i, LIBRARY_NAME), build[i])


def build_lib(build_name, generator, options, just_release):
    """ Create a dir under builds, run build and install in it """
    build_path = os.path.join(SCRIPT_PATH, 'builds', build_name)
    install_path = os.path.join(INSTALL_ROOT, build_name)
    mkdir_p(build_path)
    mkdir_p(install_path)
    with cd(build_path):
        initial_cmake = ['cmake', SCRIPT_PATH, '-DCMAKE_INSTALL_PREFIX=%s' % os.path.join('..', 'install', build_name)]
        if generator:
            initial_cmake.extend(['-G', generator])
        for key in options:
            val = options[key]
            if type(val) is bool:
                val = 'ON' if val else 'OFF'
            initial_cmake.append('-D%s=%s' % (key, val))
        click.echo('--- Building ' + build_name)
        subprocess.check_call(initial_cmake)
        if not just_release:
            subprocess.check_call(['cmake', '--build', '.', '--config', 'Debug'])
        subprocess.check_call(['cmake', '--build', '.', '--config', 'Release', '--target', 'install'])


@cli.command()
def archive():
    """ create zip of install dir """
    click.echo('--- Archiving')
    archive_file_path = os.path.join(SCRIPT_PATH, 'builds', 'discord-rpc-%s.zip' % get_platform())
    archive_file = zipfile.ZipFile(archive_file_path, 'w', zipfile.ZIP_DEFLATED)
    archive_src_base_path = INSTALL_ROOT
    archive_dst_base_path = 'discord-rpc'
    with cd(archive_src_base_path):
        for path, _, filenames in os.walk('.'):
            for fname in filenames:
                fpath = os.path.join(path, fname)
                dst_path = os.path.normpath(os.path.join(archive_dst_base_path, fpath))
                click.echo('Adding ' + dst_path)
                archive_file.write(fpath, dst_path)


@cli.command()
def sign():
    """ Do code signing within install directory using our cert """
    tool = get_signtool()
    signable_extensions = set()
    if PLATFORM == 'win':
        signable_extensions.add('.dll')
        sign_command_base = [
            tool,
            'sign',
            '/n',
            'Discord Inc.',
            '/a',
            '/tr',
            'http://timestamp.digicert.com/rfc3161',
            '/as',
            '/td',
            'sha256',
            '/fd',
            'sha256',
        ]
    elif PLATFORM == 'osx':
        signable_extensions.add('.dylib')
        sign_command_base = [
            tool,
            '--keychain',
            os.path.expanduser('~/Library/Keychains/login.keychain'),
            '-vvvv',
            '--deep',
            '--force',
            '--sign',
            'Developer ID Application: Hammer & Chisel Inc. (53Q6R32WPB)',
        ]
    else:
        click.secho('Not signing things on this platform yet', fg='red')
        return

    click.echo('--- Signing')
    for path, _, filenames in os.walk(INSTALL_ROOT):
        for fname in filenames:
            ext = os.path.splitext(fname)[1]
            if ext not in signable_extensions:
                continue
            fpath = os.path.join(path, fname)
            click.echo('Sign ' + fpath)
            sign_command = sign_command_base + [fpath]
            subprocess.check_call(sign_command)


@cli.command()
@click.option('--clean', is_flag=True)
@click.option('--static', is_flag=True)
@click.option('--shared', is_flag=True)
@click.option('--skip_formatter', is_flag=True)
@click.option('--just_release', is_flag=True)
def libs(clean, static, shared, skip_formatter, just_release):
    """ Do all the builds for this platform """
    if clean:
        shutil.rmtree('builds', ignore_errors=True)

    mkdir_p('builds')

    if not (static or shared):
        static = True
        shared = True

    static_options = {}
    dynamic_options = {
        'BUILD_SHARED_LIBS': True,
        'USE_STATIC_CRT': True,
    }

    if skip_formatter or IS_BUILD_MACHINE:
        static_options['CLANG_FORMAT_SUFFIX'] = 'none'
        dynamic_options['CLANG_FORMAT_SUFFIX'] = 'none'

    if IS_BUILD_MACHINE:
        just_release = True
        static_options['WARNINGS_AS_ERRORS'] = True
        dynamic_options['WARNINGS_AS_ERRORS'] = True

    if PLATFORM == 'win':
        generator32 = 'Visual Studio 14 2015'
        generator64 = 'Visual Studio 14 2015 Win64'
        if static:
            build_lib('win32-static', generator32, static_options, just_release)
            build_lib('win64-static', generator64, static_options, just_release)
        if shared:
            build_lib('win32-dynamic', generator32, dynamic_options, just_release)
            build_lib('win64-dynamic', generator64, dynamic_options, just_release)
    elif PLATFORM == 'osx':
        if static:
            build_lib('osx-static', None, static_options, just_release)
        if shared:
            build_lib('osx-dynamic', None, dynamic_options, just_release)
    elif PLATFORM == 'linux':
        if static:
            build_lib('linux-static', None, static_options, just_release)
        if shared:
            build_lib('linux-dynamic', None, dynamic_options, just_release)


if __name__ == '__main__':
    os.chdir(SCRIPT_PATH)
    sys.exit(cli())
