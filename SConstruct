# -*- python -*-

import os
import sys
import platform

# Home made tests
sys.path.append('SconsTests')
import wxconfig
import utils

# Some features need at least SCons 1.2
EnsureSConsVersion(1, 2)

warnings = [
    'all',
    'write-strings',
    'shadow',
    'pointer-arith',
    'packed',
    'no-conversion',
    ]

compileFlags = [
    '-fno-exceptions',
    '-fno-strict-aliasing',
    '-msse2',
    '-fPIC',
    ]

cppDefines = [
    ( '_FILE_OFFSET_BITS', 64),
    '_LARGEFILE_SOURCE',
    'GCC_HASCLASSVISIBILITY',
    ]

include_paths = [
    '#Source/Core/Common/Src',
    '#Source/Core/DiscIO/Src',
    '#Source/PluginSpecs',
    '#Source/Core/Core/Src',
    '#Source/Core/DebuggerWX/Src',
    '#Externals/Bochs_disasm',
    '#Externals/Lua',
    '#Externals/WiiUseSrc/Src',
    '#Source/Core/VideoCommon/Src',
    '#Source/Core/InputCommon/Src',
    '#Source/Core/InputUICommon/Src',
    '#Source/Core/AudioCommon/Src',
    '#Source/Core/DebuggerUICommon/Src',
    '#Source/Core/DolphinWX/Src',
    '#Source/Core/DSPCore/Src',
    ]

dirs = [
    'Externals/Bochs_disasm',
    'Externals/Lua',
    'Externals/MemcardManager',
    'Externals/WiiUseSrc/Src',
    'Source/Core/Common/Src',
    'Source/Core/Core/Src',
    'Source/Core/DiscIO/Src',
    'Source/Core/VideoCommon/Src',
    'Source/Core/InputCommon/Src',
    'Source/Core/AudioCommon/Src',
    'Source/Core/DebuggerUICommon/Src',
    'Source/Core/DSPCore/Src',
    'Source/DSPTool/Src',
    'Source/Core/InputUICommon/Src',
    'Source/Plugins/Plugin_VideoSoftware/Src',
    'Source/Plugins/Plugin_DSP_HLE/Src',
    'Source/Plugins/Plugin_DSP_LLE/Src',
    'Source/Plugins/Plugin_Wiimote/Src',
    'Source/Plugins/Plugin_WiimoteNew/Src',
    'Source/Core/DolphinWX/Src',
    'Source/Core/DebuggerWX/Src',
    'Source/UnitTests',
    ]

if sys.platform == 'darwin' or sys.platform == 'linux2':
    dirs += ['Source/Plugins/Plugin_VideoOGL/Src']

builders = {}
if sys.platform == 'darwin':
    from plistlib import writePlist
    def createPlist(target, source, env):
        properties = {}
        for srcNode in source:
            properties.update(srcNode.value)
            for dstNode in target:
                writePlist(properties, str(dstNode))
    builders['Plist'] = Builder(action = createPlist)

# Handle command line options
vars = Variables('args.cache')

vars.AddVariables(
    BoolVariable('verbose', 'Set to show compilation lines', False),
    BoolVariable('bundle', 'Set to create distribution bundle', False),
    BoolVariable('lint', 'Set for lint build (extra warnings)', False),
    BoolVariable('nowx', 'Set for building with no WX libs', False),
    EnumVariable('flavor', 'Choose a build flavor', 'release',
                 allowed_values = ('release','devel','debug','fastlog','prof'),
                 ignorecase = 2),
    PathVariable('wxconfig', 'Path to wxconfig', None),
    ('CC', 'The C compiler', 'gcc'),
    ('CXX', 'The C++ compiler', 'g++'),
    )

if not sys.platform == 'win32' and not sys.platform == 'darwin':
    vars.AddVariables(
        PathVariable('destdir',
                     'Temporary install location (for package building)',
                     None, PathVariable.PathAccept),
        EnumVariable('install',
                     'Choose a local or global installation', 'local',
                     allowed_values = ('local', 'global'), ignorecase = 2),
        PathVariable('prefix',
                     'Installation prefix (only used for a global build)',
                     '/usr', PathVariable.PathAccept),
        PathVariable('userdir',
                     'Set the name of the user data directory in home',
                     '.dolphin-emu', PathVariable.PathAccept),
        BoolVariable('opencl', 'Build with OpenCL', False),
        EnumVariable('pgo', 'Profile-Guided Optimization (generate or use)',
                     'none', allowed_values = ('none', 'generate', 'use'),
                     ignorecase = 2),
        BoolVariable('shared_glew', 'Use system shared libGLEW', True),
        BoolVariable('shared_lzo', 'Use system shared liblzo2', True),
        BoolVariable('shared_sdl', 'Use system shared libSDL', True),
        BoolVariable('shared_sfml', 'Use system shared libsfml-network', True),
        BoolVariable('shared_soil', 'Use system shared libSOIL', True),
        BoolVariable('shared_zlib', 'Use system shared libz', True),
        )

env = Environment(
    BUILDERS = builders,
    CPPPATH = include_paths,
    ENV = os.environ,
    LIBPATH = [],
    LIBS = [],
    RPATH = [],
    variables = vars,
    )

# Save the given command line options
vars.Save('args.cache', env)

# Verbose compile
if not env['verbose']:
    env['CCCOMSTR'] = "Compiling $TARGET"
    env['CXXCOMSTR'] = "Compiling $TARGET"
    env['ARCOMSTR'] = "Archiving $TARGET"
    env['LINKCOMSTR'] = "Linking $TARGET"
    env['ASCOMSTR'] = "Assembling $TARGET"
    env['ASPPCOMSTR'] = "Assembling $TARGET"
    env['SHCCCOMSTR'] = "Compiling shared $TARGET"
    env['SHCXXCOMSTR'] = "Compiling shared $TARGET"
    env['SHLINKCOMSTR'] = "Linking shared $TARGET"
    env['RANLIBCOMSTR'] = "Indexing $TARGET"

# Build flavor
flavour = env['flavor']
if (flavour == 'debug'):
    compileFlags.append('-ggdb')
    cppDefines.append('_DEBUG') #enables LOGGING
    # FIXME: this disable wx debugging how do we make it work?
    cppDefines.append('NDEBUG')
elif (flavour == 'devel'):
    compileFlags.append('-ggdb')
elif (flavour == 'fastlog'):
    compileFlags.append('-O3')
    cppDefines.append('DEBUGFAST')
elif (flavour == 'prof'):
    compileFlags.append('-O3')
    compileFlags.append('-ggdb')
elif (flavour == 'release'):
    compileFlags.append('-O3')
    compileFlags.append('-fomit-frame-pointer');
# More warnings
if env['lint']:
    warnings.append('error')
    # Should check for the availability of these (in GCC 4.3 or newer)
    if sys.platform != 'darwin':
        warnings.append('no-array-bounds')
        warnings.append('no-unused-result')
    # wxWidgets causes too many warnings with these
    #warnings.append('unreachable-code')
    #warnings.append('float-equal')

# Add the warnings to the compile flags
compileFlags += [ ('-W' + warning) for warning in warnings ]

env['CCFLAGS'] = compileFlags
env['CXXFLAGS'] = ['-fvisibility-inlines-hidden']
env['CPPDEFINES'] = cppDefines

# Configuration tests section
tests = {'CheckWXConfig' : wxconfig.CheckWXConfig,
         'CheckPKGConfig' : utils.CheckPKGConfig,
         'CheckPKG' : utils.CheckPKG,
         'CheckSDL' : utils.CheckSDL,
         'CheckPortaudio' : utils.CheckPortaudio,
         }

# Object files
env['build_dir'] = os.path.join('Build',
    platform.system() + '-' + platform.machine() + '-' + env['flavor'] + os.sep)

# Static libs go here
env['local_libs'] = '#' + env['build_dir'] + os.sep + 'libs' + os.sep

# Install paths
extra=''
if flavour == 'debug':
    extra = '-debug'
elif flavour == 'prof':
    extra = '-prof'

# Set up the install locations
if sys.platform == 'linux2' and env['install'] == 'global':
    env['prefix'] = os.path.join(env['prefix'] + os.sep)
    env['binary_dir'] = env['prefix'] + 'bin/'
    env['plugin_dir'] = env['prefix'] + 'lib/dolphin-emu/'
    env['data_dir'] = env['prefix'] + "share/dolphin-emu/"
else:
    env['prefix'] = os.path.join('Binary',
        platform.system() + '-' + platform.machine() + extra + os.sep)
    env['binary_dir'] = '#' + env['prefix']
    env['plugin_dir'] = '#' + env['prefix'] + 'plugins/'
    env['data_dir'] = '#' + env['prefix']
if sys.platform == 'darwin':
    env['plugin_dir'] = '#' + env['prefix'] + 'Dolphin.app/Contents/PlugIns/'
    env['data_dir'] = '#' + env['prefix'] + 'Dolphin.app/Contents/Resources'

shared = {}
shared['glew'] = shared['lzo'] = shared['sdl'] = \
shared['soil'] = shared['sfml'] = shared['zlib'] = 0
wxmods = ['aui', 'adv', 'core', 'base', 'gl']
env['HAVE_OPENCL'] = 0
env['HAVE_WX'] = 0

# OS X specifics
if sys.platform == 'darwin':
    env['CCFLAGS'] += ['-arch', 'x86_64', '-arch', 'i386']
    env['CCFLAGS'] += ['-mmacosx-version-min=10.5']
    env['CC'] = "gcc-4.2"
    env['CFLAGS'] += ['-x', 'objective-c']
    env['CXX'] = "g++-4.2"
    env['CXXFLAGS'] += ['-x', 'objective-c++']
    env['FRAMEWORKS'] += ['AppKit', 'CoreFoundation', 'CoreServices']
    env['FRAMEWORKS'] += ['AudioUnit', 'CoreAudio']
    env['FRAMEWORKS'] += ['IOBluetooth', 'IOKit', 'OpenGL']
    env['LIBS'] += ['iconv']
    env['LINKFLAGS'] += ['-arch', 'x86_64', '-arch', 'i386']
    env['LINKFLAGS'] += ['-mmacosx-version-min=10.5']
    env['LINKFLAGS'] += ['-Z', '-L/Developer/SDKs/MacOSX10.5.sdk/usr/lib',
        '-F/Developer/SDKs/MacOSX10.5.sdk/System/Library/Frameworks',
        '-F/Developer/SDKs/MacOSX10.6.sdk/System/Library/Frameworks']
    if platform.mac_ver()[0] >= '10.6.0':
        env['HAVE_OPENCL'] = 1
        env['LINKFLAGS'] += ['-weak_framework', 'OpenCL']
    if not env['nowx']:
        frameworks = env['FRAMEWORKS']
        conf = env.Configure(custom_tests = tests)
        env['HAVE_WX'] = conf.CheckWXConfig(2.9, wxmods, 0)
        conf.Finish()
        # wx-config wants us to link with the OS X QuickTime framework
        # which is not available for x86_64 and we don't use it anyway.
        # Strip it out to silence some harmless linker warnings.
        # In the 10.5 SDK, Carbon is only partially built for x86_64.
        wxconfig.ParseWXConfig(env)
        if env['CPPDEFINES'].count('WXUSINGDLL'):
            env['FRAMEWORKS'] = frameworks
        #    if env['FRAMEWORKS'].count('AudioToolbox'):
        #        env['FRAMEWORKS'].remove('AudioToolbox')
        #    if env['FRAMEWORKS'].count('Carbon'):
        #        env['FRAMEWORKS'].remove('Carbon')
        #    if env['FRAMEWORKS'].count('System'):
        #        env['FRAMEWORKS'].remove('System')
        #    if env['FRAMEWORKS'].count('QuickTime'):
        #        env['FRAMEWORKS'].remove('QuickTime')
    env['CPPPATH'] += ['#Externals']
    env['FRAMEWORKS'] += ['Cg']
    env['LINKFLAGS'] += ['-FExternals/Cg']
    shared['zlib'] = 1

elif sys.platform == 'win32':
    env['tools'] = ['mingw']

else:
    env['CPPPATH'].insert(0, '#')
    env['LINKFLAGS'] += ['-pthread']
    conf = env.Configure(custom_tests = tests, config_h="#config.h")

    if not conf.CheckPKGConfig('0.15.0'):
        print "Can't find pkg-config, some tests will fail"

    if env['shared_glew']:
        shared['glew'] = conf.CheckPKG('GLEW')
    if env['shared_sdl']:
        shared['sdl'] = conf.CheckPKG('SDL')
    if env['shared_zlib']:
        shared['zlib'] = conf.CheckPKG('z')
    if env['shared_lzo']:
        shared['lzo'] = conf.CheckPKG('lzo2')
    # TODO:  Check the version of sfml.  It should be at least version 1.5
    if env['shared_sfml']:
        shared['sfml'] = conf.CheckPKG('sfml-network') and \
                         conf.CheckCXXHeader("SFML/Network/Ftp.hpp")
    if env['shared_soil']:
        shared['soil'] = conf.CheckPKG('SOIL')
    for lib in shared:
        if not shared[lib]:
            print "Shared library " + lib + " not detected, " \
                  "falling back to the static library"

    if not env['nowx']:
        # wxGLCanvas does not play well with wxGTK
        wxmods.remove('gl')
        env['HAVE_WX'] = conf.CheckWXConfig(2.8, wxmods, 0)
        conf.Define('HAVE_WX', env['HAVE_WX'])
        wxconfig.ParseWXConfig(env)
        if not env['HAVE_WX']:
            print "WX libraries not found - see config.log"
            Exit(1)

    conf.CheckPKG('usbhid')

    env['HAVE_BLUEZ'] = conf.CheckPKG('bluez')
    conf.Define('HAVE_BLUEZ', env['HAVE_BLUEZ'])

    env['HAVE_ALSA'] = conf.CheckPKG('alsa')
    conf.Define('HAVE_ALSA', env['HAVE_ALSA'])

    env['HAVE_AO'] = conf.CheckPKG('ao')
    conf.Define('HAVE_AO', env['HAVE_AO'])

    env['HAVE_OPENAL'] = conf.CheckPKG('openal')
    conf.Define('HAVE_OPENAL', env['HAVE_OPENAL'])

    env['HAVE_PORTAUDIO'] = conf.CheckPortaudio(1890)
    conf.Define('HAVE_PORTAUDIO', env['HAVE_PORTAUDIO'])

    env['HAVE_PULSEAUDIO'] = conf.CheckPKG('libpulse-simple')
    conf.Define('HAVE_PULSEAUDIO', env['HAVE_PULSEAUDIO'])

    env['HAVE_X11'] = conf.CheckPKG('x11')
    env['HAVE_XRANDR'] = env['HAVE_X11'] and conf.CheckPKG('xrandr')
    conf.Define('HAVE_XRANDR', env['HAVE_XRANDR'])
    conf.Define('HAVE_X11', env['HAVE_X11'])

    # Check for GTK 2.0 or newer
    if env['HAVE_WX'] and not conf.CheckPKG('gtk+-2.0'):
        print "gtk+-2.0 developement headers not detected"
        print "gtk+-2.0 is required to build the WX GUI"
        Exit(1)

    if not conf.CheckPKG('GL'):
        print "Must have OpenGL to build"
        Exit(1)
    if not conf.CheckPKG('GLU'):
        print "Must have GLU to build"
        Exit(1)

    if sys.platform == 'linux2':
        if not conf.CheckPKG('Cg'):
            print "Must have Cg toolkit from NVidia to build"
            Exit(1)
        if not conf.CheckPKG('CgGL'):
            print "Must have CgGl to build"
            Exit(1)

    if env['opencl']:
        env['HAVE_OPENCL'] = conf.CheckPKG('OpenCL')
        conf.Define('HAVE_OPENCL', env['HAVE_OPENCL'])

    conf.Define('USER_DIR', "\"" + env['userdir'] + "\"")
    if (env['install'] == 'global'):
        conf.Define('DATA_DIR', "\"" + env['data_dir'] + "\"")
        conf.Define('LIBS_DIR', "\"" + env['prefix'] + 'lib/' +  "\"")

    # PGO - Profile Guided Optimization
    if env['pgo']=='generate':
        compileFlags.append('-fprofile-generate')
        env['LINKFLAGS']='-fprofile-generate'
    if env['pgo']=='use':
        compileFlags.append('-fprofile-use')
        env['LINKFLAGS']='-fprofile-use'

    # Profiling
    if (flavour == 'prof'):
        proflibs = [ '/usr/lib/oprofile', '/usr/local/lib/oprofile' ]
        env['LIBPATH'].append(proflibs)
        env['RPATH'].append(proflibs)
        if conf.CheckPKG('opagent'):
            conf.Define('USE_OPROFILE', 1)
        else:
            print "Can't build prof without oprofile, disabling"

    # After all configuration tests are done
    conf.Finish()

# Local (static) libraries must be first in the search path for the build in
# order that they can override system libraries, but they must not be found
# during autoconfiguration as they will then be detected as system libraries.
env['LIBPATH'].insert(0, env['local_libs'])

if not shared['glew']:
    env['CPPPATH'] += ['#Externals/GLew/include']
    dirs += ['Externals/GLew']
if not shared['lzo']:
    env['CPPPATH'] += ['#Externals/LZO']
    dirs += ['Externals/LZO']
if not shared['sdl']:
    env['CPPPATH'] += ['#Externals/SDL']
    env['CPPPATH'] += ['#Externals/SDL/include']
    dirs += ['Externals/SDL']
if not shared['soil']:
    env['CPPPATH'] += ['#Externals/SOIL']
    dirs += ['Externals/SOIL']
if not shared['sfml']:
    env['CPPPATH'] += ['#Externals/SFML/include']
    dirs += ['Externals/SFML/src']
if not shared['zlib']:
    env['CPPPATH'] += ['#Externals/zlib']
    dirs += ['Externals/zlib']

rev = utils.GenerateRevFile(env['flavor'],
                            "Source/Core/Common/Src/svnrev_template.h",
                            "Source/Core/Common/Src/svnrev.h")

# Print a nice progress indication when not compiling
Progress(['-\r', '\\\r', '|\r', '/\r'], interval=5)

# Setup destdir for package building
# Warning:  The program will not run from this location.  It is assumed the
# package will later install it to the prefix as it was defined before this.
if env.has_key('destdir'):
    env['prefix'] = env['destdir'] + env['prefix']
    env['binary_dir'] = env['destdir'] + env['binary_dir']
    env['plugin_dir'] = env['destdir'] + env['plugin_dir']
    env['data_dir'] = env['destdir'] + env['data_dir']

# Die on unknown variables
unknown = vars.UnknownVariables()
if unknown:
    print "Unknown variables:", unknown.keys()
    Exit(1)

# Generate help
Help(vars.GenerateHelpText(env))

Export('env')

for subdir in dirs:
    SConscript(
        subdir + os.sep + 'SConscript',
        variant_dir = env['build_dir'] + subdir + os.sep,
        duplicate=0
        )

# Data install
if sys.platform == 'darwin':
    env.Install(env['data_dir'], 'Data/Sys')
    env.Install(env['data_dir'], 'Data/User')
else:
    env.InstallAs(env['data_dir'] + 'sys', 'Data/Sys')
    env.InstallAs(env['data_dir'] + 'user', 'Data/User')

env.Alias('install', env['prefix'])

if env['bundle']:
    if sys.platform == 'linux2':
        # Make tar ball (TODO put inside normal dir)
        tar_env = env.Clone()
        tarball = tar_env.Tar('dolphin-' + rev + '.tar.bz2', env['prefix'])
        tar_env.Append(TARFLAGS='-j', TARCOMSTR="Creating release tarball")
        env.Clean(all, tarball)
    elif sys.platform == 'darwin':
        app = env['prefix'] + 'Dolphin.app'
        dmg = env['prefix'] + 'Dolphin-r' + rev + '.dmg'
        env.Command(dmg, app + '/Contents/MacOS/Dolphin', 'rm -f ' + dmg +
            ' && hdiutil create -srcfolder ' + app + ' -format UDBZ ' + dmg +
            ' && hdiutil internet-enable -yes ' + dmg)
