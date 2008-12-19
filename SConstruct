# -*- python -*- 

import os
import sys
import platform

# Home made tests
sys.path.append('SconsTests')
import wxconfig 
import utils

# Some features needs at least scons 0.98
EnsureSConsVersion(0, 98)

# TODO: how do we use it in help?
name="Dolphin"
version="SVN"
description="A wii/gamecube emulator"
license="GPL v2"

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
    '-fvisibility=hidden',
    #'-fomit-frame-pointer'
    ]

cppDefines = [
    ( '_FILE_OFFSET_BITS', 64),
    '_LARGEFILE_SOURCE',
    'GCC_HASCLASSVISIBILITY',
    ]

include_paths = [
    '../../../Core/Common/Src',
    '../../../Core/DiscIO/Src',
    '../../../PluginSpecs',
    '../../../',
    '../../../Core/Core/Src',
    '../../../Core/DebuggerWX/Src',
    '../../../../Externals/Bochs_disasm',
    '../../../../Externals/LZO',
    '../../../../Externals/WiiUseSrc/Src',
    '../../../Core/VideoCommon/Src',
    ]

dirs = [
    'Externals/Bochs_disasm',
    'Externals/LZO',
    'Externals/WiiUseSrc/Src', 
    'Source/Core/Common/Src',
    'Source/Core/Core/Src',
    'Source/Core/DiscIO/Src',
    'Source/Core/VideoCommon/Src',
    'Source/Plugins/Plugin_VideoOGL/Src',
    'Source/Plugins/Plugin_DSP_HLE/Src',
    'Source/Plugins/Plugin_DSP_LLE/Src',
    'Source/Plugins/Plugin_DSP_NULL/Src',
    'Source/Plugins/Plugin_PadSimple/Src',
    'Source/Plugins/Plugin_nJoy_SDL/Src',
    'Source/Plugins/Plugin_nJoy_Testing/Src',
    'Source/Plugins/Plugin_Wiimote/Src',
    'Source/Core/DolphinWX/Src',
    'Source/Core/DebuggerWX/Src',
    ]

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
    compileFlags += [ '-I/opt/local/include' ]

lib_paths = include_paths

# handle command line options
vars = Variables('args.cache')

vars.AddVariables(
    BoolVariable('verbose', 'Set for compilation line', False),
    BoolVariable('bundle', 'Set to create bundle', False),
    BoolVariable('lint', 'Set for lint build (extra warnings)', False),
    BoolVariable('nowx', 'Set For Building with no WX libs (WIP)', False),
    BoolVariable('wxgl', 'Set For Building with WX GL libs (WIP)', False),
    BoolVariable('sdlgl', 'Set For Building with SDL GL libs (WIP)', False),
    EnumVariable('flavor', 'Choose a build flavor', 'release',
                 allowed_values = ('release', 'devel', 'debug', 'fastlog'),
                 ignorecase = 2
                 ),
    EnumVariable('osx', 'Choose a backend (WIP)', '32cocoa',
                 allowed_values = ('32x11', '32cocoa', '64cocoa'),
                 ignorecase = 2
                 ),
    PathVariable('wxconfig', 'Path to the wxconfig', None),
    ('CC', 'The c compiler', 'gcc'),
    ('CXX', 'The c++ compiler', 'g++'),
    )


env = Environment(
    CPPPATH = include_paths,
    LIBPATH = lib_paths,
    variables = vars,
    ENV = {
        'PATH' : os.environ['PATH'],
        'HOME' : os.environ['HOME']
        },
    BUILDERS = builders,
    DESCRIPTION = description,
    SUMMARY = description,
    LICENSE = license,
    NAME = name,
    VERSION = version,
    )

# save the given command line options
vars.Save('args.cache', env)

# verbose compile
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


# build falvuor
flavour = ARGUMENTS.get('flavor')
if (flavour == 'debug'):
    compileFlags.append('-g')
    cppDefines.append('LOGGING')
    cppDefines.append('_DEBUG')
    # FIXME: this disable wx debugging how do we make it work?
    cppDefines.append('NDEBUG') 

elif (flavour == 'devel'):
    compileFlags.append('-g')
    cppDefines.append('DEBUGFAST')
elif (flavour == 'fastlog'):
    compileFlags.append('-O3')
    cppDefines.append('LOGGING')
else:
    compileFlags.append('-O3')

# more warnings
if env['lint']:
    warnings.append('error')
    warnings.append('unreachable-code')
    warnings.append('float-equal')

# add the warnings to the compile flags
compileFlags += [ '-W' + warning for warning in warnings ]

env['CCFLAGS'] = compileFlags
env['CXXFLAGS'] = compileFlags + [ '-fvisibility-inlines-hidden' ]
env['CPPDEFINES'] = cppDefines


# Configuration tests section
tests = {'CheckWXConfig' : wxconfig.CheckWXConfig,
         'CheckPKGConfig' : utils.CheckPKGConfig,
         'CheckPKG' : utils.CheckPKG,
         'CheckSDL' : utils.CheckSDL}

build_dir = os.path.join('Build', platform.system() + '-' + platform.machine() + os.sep)
VariantDir(build_dir, '.', duplicate=0)

conf = env.Configure(custom_tests = tests, 
                     config_h="Source/Core/Common/Src/Config.h")

if not conf.CheckPKGConfig('0.15.0'):
    print "Can't find pkg-config, some tests will fail"

env['HAVE_SDL'] = conf.CheckSDL('1.0.0')

# Bluetooth for wii support
env['HAVE_BLUEZ'] = conf.CheckPKG('bluez')

# needed for sound
env['HAVE_AO'] = conf.CheckPKG('ao')

#osx 64 specifics
if sys.platform == 'darwin':
    if env['osx'] == '64cocoa':
        compileFlags += ['-arch' , 'x86_64' ]
        conf.Define('MAP_32BIT', 0)

    if not env['osx'] == '32x11':
        env['HAVE_X11'] = 0
        env['HAVE_COCOA'] = conf.CheckPKG('cocoa')
else:
    env['HAVE_X11'] = conf.CheckPKG('x11')
    env['HAVE_COCOA'] = 0
   
# handling wx flags CCFLAGS should be created before
wxmods = ['adv', 'core', 'base']
env['USE_WX'] = 0
if env['wxgl']:
    wxmods.append('gl')
    env['USE_WX'] = 1
    env['HAVE_X11'] = 0
    env['HAVE_COCOA'] = 0

env['HAVE_WX'] = conf.CheckWXConfig('2.8', wxmods, 0) 

# SDL backend
env['USE_SDL'] = 0

if env['sdlgl']:
    env['USE_SDL'] = 1
    env['HAVE_X11'] = 0
    env['HAVE_COCOA'] = 0
    env['USE_WX'] = 0

# Gui less build
if env['nowx']:
    env['HAVE_WX'] = 0;

# Creating config.h defines
conf.Define('HAVE_SDL', env['HAVE_SDL'])
conf.Define('USE_SDL', env['USE_SDL'])
conf.Define('HAVE_BLUEZ', env['HAVE_BLUEZ'])
conf.Define('HAVE_AO', env['HAVE_AO'])
conf.Define('HAVE_WX', env['HAVE_WX'])
conf.Define('USE_WX', env['USE_WX'])
conf.Define('HAVE_X11', env['HAVE_X11'])
conf.Define('HAVE_COCOA', env['HAVE_COCOA'])

# After all configuration tests are done
conf.Finish()

#wx windows flags
if env['HAVE_WX']:
    wxconfig.ParseWXConfig(env)
else:
    print "WX not found or disabled, not building GUI"

# add methods from utils to env
env.AddMethod(utils.filterWarnings)

# Where do we run from
env['base_dir'] = os.getcwd()+ '/';

# install paths
# TODO: support global install
env['prefix'] = os.path.join(env['base_dir'] + 'Binary', platform.system() + '-' + platform.machine() + '/')
#TODO add lib
env['plugin_dir'] = env['prefix'] + 'Plugins/' 
#TODO add bin
env['binary_dir'] = env['prefix']
#TODO add bin
env['libs_dir'] = env['prefix'] + 'Libs/'
#TODO where should this go?
env['data_dir'] = env['prefix']

env['RPATH'] =  env['libs_dir']

env['LIBPATH'] += [ env['libs_dir'] ] 


rev = utils.GenerateRevFile(env['flavor'], 
                            "Source/Core/Common/Src/svnrev_template.h",
                            "Source/Core/Common/Src/svnrev.h")
# print a nice progress indication when not compiling
Progress(['-\r', '\\\r', '|\r', '/\r'], interval = 5)

# die on unknown variables
unknown = vars.UnknownVariables()
if unknown:
    print "Unknown variables:", unknown.keys()
    Exit(1)

# generate help
Help(vars.GenerateHelpText(env))

Export('env')

for subdir in dirs:
    SConscript(
        subdir + os.sep + 'SConscript',
        variant_dir = build_dir + subdir + os.sep,
        duplicate=0
        )

# Data install
env.Install(env['data_dir'], 'Data/Sys')
env.Install(env['data_dir'], 'Data/User')

if sys.platform == 'darwin':
    env.Install(env['binary_dir'] + 'Dolphin.app/Contents/Resources/', 
                'Source/Core/DolphinWX/resources/Dolphin.icns')

if env['bundle']:
    # Make tar ball (TODO put inside normal dir)
    tar_env = env.Clone()
    tarball = tar_env.Tar('dolphin-'+rev +'.tar.bz2', env['prefix'])
    tar_env.Append(TARFLAGS='-j', 
                   TARCOMSTR="Creating release tarball")


#TODO clean all bundles
#env.Clean(all, 'dolphin-*'+ '.tar.bz2')


