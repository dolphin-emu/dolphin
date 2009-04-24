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
    #'-fomit-frame-pointer'
    ]
if sys.platform != 'win32':
    compileFlags += [ '-fvisibility=hidden' ]

cppDefines = [
    ( '_FILE_OFFSET_BITS', 64),
    '_LARGEFILE_SOURCE',
    'GCC_HASCLASSVISIBILITY',
    ]

basedir = os.getcwd()+ '/'

include_paths = [
    basedir + 'Source/Core/Common/Src',
    basedir + 'Source/Core/DiscIO/Src',
    basedir + 'Source/PluginSpecs',
    basedir + 'Source/Core/Core/Src',
    basedir + 'Source/Core/DebuggerWX/Src',
    basedir + 'Externals/Bochs_disasm',
    basedir + 'Externals/LZO',
    basedir + 'Externals/WiiUseSrc/Src',
    basedir + 'Source/Core/VideoCommon/Src',
    basedir + 'Source/Core/InputCommon/Src',
    basedir + 'Source/Core/AudioCommon/Src',
    basedir + 'Source/Core/DSPCore/Src',
    ]

dirs = [
    'Externals/Bochs_disasm',
    'Externals/LZO',
    'Externals/WiiUseSrc/Src', 
    'Source/Core/Common/Src',
    'Source/Core/Core/Src',
    'Source/Core/DiscIO/Src',
    'Source/Core/VideoCommon/Src',
    'Source/Core/InputCommon/Src',
    'Source/Core/AudioCommon/Src',
    'Source/Core/DSPCore/Src',
    'Source/DSPTool/Src',
    'Source/Plugins/Plugin_VideoOGL/Src',
    'Source/Plugins/Plugin_DSP_HLE/Src',
    'Source/Plugins/Plugin_DSP_LLE/Src',
    'Source/Plugins/Plugin_PadSimple/Src',
    'Source/Plugins/Plugin_PadSimpleEvnt/Src',
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

# handle command line options
vars = Variables('args.cache')

vars.AddVariables(
    BoolVariable('verbose', 'Set for compilation line', False),
    BoolVariable('bundle', 'Set to create bundle', False),
    BoolVariable('lint', 'Set for lint build (extra warnings)', False),
    BoolVariable('nowx', 'Set For Building with no WX libs (WIP)', False),
    BoolVariable('wxgl', 'Set For Building with WX GL libs (WIP)', False),
    BoolVariable('sdlgl', 'Set For Building with SDL GL libs (WIP)', False),
    BoolVariable('gltest', 'temp don\'t use (WIP)', False),
    BoolVariable('jittest', 'temp don\'t use (WIP)', False),
    EnumVariable('flavor', 'Choose a build flavor', 'release',
                 allowed_values = ('release', 'devel', 'debug', 'fastlog', 'prof'),
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

if sys.platform == 'win32':
    env = Environment(
        CPPPATH = include_paths,
        RPATH = [],
        LIBS = [],
        LIBPATH = [],
        tools = [ 'mingw' ],
        variables = vars,
        ENV = os.environ,
        BUILDERS = builders,
        DESCRIPTION = description,
        SUMMARY = description,
        LICENSE = license,
        NAME = name,
        VERSION = version,
        )
else:
    env = Environment(
        CPPPATH = include_paths,
        RPATH = [],
        LIBS = [],
        LIBPATH = [],
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
    cppDefines.append('_DEBUG') #enables LOGGING
    # FIXME: this disable wx debugging how do we make it work?
    cppDefines.append('NDEBUG') 
elif (flavour == 'devel'):
    compileFlags.append('-g')
elif (flavour == 'fastlog'):
    compileFlags.append('-O3')
    cppDefines.append('DEBUGFAST')
elif (flavour == 'prof'):
    compileFlags.append('-O3')
    compileFlags.append('-g')
elif (flavour == 'release'):
    compileFlags.append('-O3')

# more warnings
if env['lint']:
    warnings.append('error')
    warnings.append('unreachable-code')
    warnings.append('float-equal')

# add the warnings to the compile flags
compileFlags += [ '-W' + warning for warning in warnings ]

env['CCFLAGS'] = compileFlags
if sys.platform == 'win32':
    env['CXXFLAGS'] = compileFlags
else:
    env['CXXFLAGS'] = compileFlags + [ '-fvisibility-inlines-hidden' ]
env['CPPDEFINES'] = cppDefines


# Configuration tests section
tests = {'CheckWXConfig' : wxconfig.CheckWXConfig,
         'CheckPKGConfig' : utils.CheckPKGConfig,
         'CheckPKG' : utils.CheckPKG,
         'CheckSDL' : utils.CheckSDL,
         'CheckFink' : utils.CheckFink,
         'CheckMacports' : utils.CheckMacports,
         'CheckPortaudio' : utils.CheckPortaudio,
         }

#object files
env['build_dir'] = os.path.join(basedir, 'Build', platform.system() + '-' + platform.machine() + '-' + env['flavor'] + os.sep)


VariantDir(env['build_dir'], '.', duplicate=0)

conf = env.Configure(custom_tests = tests, 
                     config_h="Source/Core/Common/Src/Config.h")

if not conf.CheckPKGConfig('0.15.0'):
    print "Can't find pkg-config, some tests will fail"

# find ports/fink for library and include path
if sys.platform == 'darwin':
    #ports usually has newer versions
    conf.CheckMacports()
    conf.CheckFink()

env['HAVE_SDL'] = conf.CheckSDL('1.0.0')

# Bluetooth for wii support
env['HAVE_BLUEZ'] = conf.CheckPKG('bluez')

# needed for sound
env['HAVE_AO'] = conf.CheckPKG('ao')

# Sound lib
env['HAVE_OPENAL'] = conf.CheckPKG('openal')

if sys.platform != 'darwin':
# needed for mic
    env['HAVE_PORTAUDIO'] =  conf.CheckPortaudio(1890)
else:
    env['HAVE_PORTAUDIO'] =  0

# sfml
env['HAVE_SFML'] = 0
if conf.CheckPKG('sfml') and conf.CheckCHeader("SFML/Audio.hpp"):
    env['HAVE_SFML'] = 1;

#osx 64 specifics
if sys.platform == 'darwin':
    if env['osx'] == '64cocoa':
        env['nowx'] = True
        compileFlags += ['-arch' , 'x86_64' ]
        conf.Define('MAP_32BIT', 0)

    if not env['osx'] == '32x11':
        env['HAVE_X11'] = 0
        env['HAVE_COCOA'] = 1

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

if sys.platform == 'win32':
    env['HAVE_WX'] = 1
    env['USE_WX'] = 1

# Gui less build
if env['nowx']:
    env['HAVE_WX'] = 0;
else:
    env['HAVE_WX'] = conf.CheckWXConfig('2.8', wxmods, 0) 

# SDL backend
env['USE_SDL'] = 0

if env['sdlgl']:
    env['USE_SDL'] = 1
    env['HAVE_X11'] = 0
    env['HAVE_COCOA'] = 0
    env['USE_WX'] = 0

env['GLTEST'] = 0
if env['gltest']:
    env['GLTEST'] = 1

conf.Define('GLTEST', env['GLTEST'])

env['JITTEST'] = 0
if env['jittest']:
    env['JITTEST'] = 1

conf.Define('JITTEST', env['JITTEST'])

# Creating config.h defines
conf.Define('HAVE_SDL', env['HAVE_SDL'])
conf.Define('USE_SDL', env['USE_SDL'])
conf.Define('HAVE_BLUEZ', env['HAVE_BLUEZ'])
conf.Define('HAVE_AO', env['HAVE_AO'])
conf.Define('HAVE_OPENAL', env['HAVE_OPENAL'])
conf.Define('HAVE_WX', env['HAVE_WX'])
conf.Define('USE_WX', env['USE_WX'])
conf.Define('HAVE_X11', env['HAVE_X11'])
conf.Define('HAVE_COCOA', env['HAVE_COCOA'])
conf.Define('HAVE_PORTAUDIO', env['HAVE_PORTAUDIO'])
conf.Define('HAVE_SFML', env['HAVE_SFML'])

# profile
env['USE_OPROFILE'] = 0
if (flavour == 'prof'): 
    proflibs = [ '/usr/lib/oprofile', '/usr/local/lib/oprofile' ]
    env['LIBPATH'].append(proflibs)
    env['RPATH'].append(proflibs)
    if conf.CheckPKG('opagent'):
        env['USE_OPROFILE'] = 1
    else:
        print "Can't build prof without oprofile, disabling"

conf.Define('USE_OPROFILE', env['USE_OPROFILE'])
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
env['base_dir'] = os.getcwd()+ '/'

# install paths
extra=''
if flavour == 'debug':
    extra = '-debug'
elif flavour == 'prof':
    extra = '-prof'

# TODO: support global install
env['prefix'] = os.path.join(env['base_dir'] + 'Binary', platform.system() + '-' + platform.machine() + extra +os.sep)
#TODO add lib
if sys.platform == 'darwin':
    env['plugin_dir'] = env['prefix'] + 'Dolphin.app/Contents/PlugIns/'
else:
    env['plugin_dir'] = env['prefix'] + 'Plugins/' 
#TODO add bin
env['binary_dir'] = env['prefix']
#TODO add bin
env['libs_dir'] = env['prefix'] + 'Libs/'
#TODO where should this go?
env['data_dir'] = env['prefix']

env['RPATH'].append(env['libs_dir'])

# static libs goes here
env['local_libs'] =  env['build_dir'] + os.sep + 'libs' + os.sep

env['LIBPATH'].append(env['local_libs']) 
env['LIBPATH'].append(env['libs_dir']) 


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
        variant_dir = env[ 'build_dir' ] + subdir + os.sep,
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


