# -*- python -*- 

import os
import sys

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


if sys.platform == 'darwin':
    compileFlags += [ '-I/opt/local/include' ]

include_paths = [
    '../../../Core/Common/Src',
    '../../../Core/DiscIO/Src',
    '../../../PluginSpecs',
    '../../../',
    '../../../Core/Core/Src',
    '../../../Core/DebuggerWX/Src',
    '../../../../Externals/Bochs_disasm',
    '../../../../Externals/LZO',
    '../../../Core/VideoCommon/Src',
    ]

dirs = [
    'Source/Core/Common/Src',
    'Externals/Bochs_disasm',
    'Externals/LZO',
    'Source/Core/Core/Src',
    'Source/Core/DiscIO/Src',
    'Source/Core/VideoCommon/Src',
    'Source/Plugins/Plugin_VideoOGL/Src',
    'Source/Plugins/Plugin_DSP_HLE/Src',
    'Source/Plugins/Plugin_DSP_LLE/Src',
    'Source/Plugins/Plugin_DSP_NULL/Src',
    'Source/Plugins/Plugin_PadSimple/Src',
    'Source/Plugins/Plugin_nJoy_SDL/Src',
    'Source/Plugins/Plugin_Wiimote_Test/Src',
    'Source/Core/DolphinWX/Src',
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

lib_paths = include_paths

# handle command line options
vars = Variables('args.cache')
vars.AddVariables(
    BoolVariable('verbose', 'Set for compilation line', False),
    BoolVariable('debug', 'Set for debug build', False),
    BoolVariable('lint', 'Set for lint build (extra warnings)', False),
    BoolVariable('nowx', 'Set For Building with no WX libs (WIP)', False),
    EnumVariable('flavor', 'Choose a build flavor', 'release',
                 allowed_values = ('release', 'devel', 'debug'),
                 ignorecase = 2
                 )
    )

env = Environment(
    CC = 'gcc',
    CXX = 'g++',
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
elif (flavour == 'devel'):
    compileFlags.append('-g')
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

conf = env.Configure(custom_tests = tests)

if not conf.CheckPKGConfig('0.15.0'):
    Exit(1)

if not conf.CheckSDL('1.0.0'):
    Exit(1)

if not conf.CheckPKG('ao'):
    Exit(1)

# handling wx flags CCFLAGS should be created before
if not env['nowx']:
    if not conf.CheckWXConfig(
        '2.8', ['gl', 'adv', 'core', 'base'], env['debug']
        ):
        print 'gui build requires wxwidgets >= 2.8'
        Exit(1)
if not env['nowx']:
    dirs += ['Source/Core/DebuggerWX/Src',]

# After all configuration tests are done
env = conf.Finish()

#wx windows flags
if not env['nowx']:
    wxconfig.ParseWXConfig(env)
    compileFlags += ['-DUSE_WX',]

#get sdl stuff
env.ParseConfig('sdl-config --cflags --libs')

# lib ao (needed for sound plugins)
env.ParseConfig('pkg-config --cflags --libs ao')

# add methods from utils to env
env.AddMethod(utils.filterWarnings)

Export('env')

utils.GenerateRevFile(env['flavor'], "Source/Core/Common/Src/svnrev_template.h",
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

for subdir in dirs:
    SConscript(
        subdir + os.sep + 'SConscript',
        duplicate = 0
        )
