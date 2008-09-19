# -*- python -*- 
import os
import sys

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
#        '-fomit-frame-pointer'
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
	'../../../Core/DebuggerWX/src',
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
	'Source/Core/DebuggerWX/src',
	'Source/Core/VideoCommon/Src',
	'Source/Plugins/Plugin_VideoOGL/Src',
	'Source/Plugins/Plugin_DSP_HLE/Src',
	'Source/Plugins/Plugin_DSP_LLE/Src',
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
vars = Variables('custom.py')
vars.AddVariables(
        BoolVariable('verbose', 'Set for compilation line', False),
        BoolVariable('debug', 'Set for debug build', False),
        BoolVariable('lint', 'Set for lint build (extra warnings)', False),
        EnumVariable('flavor', 'Choose a build flavor', 'release',
                     allowed_values=('release', 'devel', 'debug'),
                     ignorecase=2)

)

# build falvuor
flavour =  ARGUMENTS.get('flavor')
if (flavour == 'debug'):
	compileFlags.append('-g')
        cppDefines.append('LOGGING')
else:
	compileFlags.append('-O3')


# more warnings
lint = ARGUMENTS.get('lint', False)
if bool(lint):
	warnings.append('error')
        warnings.append('unreachable-code')
	warnings.append('float-equal')


compileFlags += [ '-W' + warning for warning in warnings ]

env = Environment(
	CC = 'gcc',
	CXX = 'g++',
	CCFLAGS = ' '.join(compileFlags),
	CXXFLAGS = ' '.join(compileFlags + [ '-fvisibility-inlines-hidden' ]),
        CPPDEFINES = cppDefines,
	CPPPATH = include_paths,
	LIBPATH = lib_paths,
        variables = vars,
	ENV = {
		'PATH' : os.environ['PATH'],
		'HOME' : os.environ['HOME']
		},
	BUILDERS = builders,
        DESCRIPTION = description,
        SUMMARY=description,
        LICENSE = license,
        NAME = name,
        VERSION = version,
	)

# verbose compile
verbose = ARGUMENTS.get('verbose', False)

if not verbose:
        env['CCCOMSTR'] = "Compiling $TARGET"
        env['CXXCOMSTR'] = "Compiling $TARGET"
        env['ARCOMSTR'] = " ar $TARGER"
        env['LINKCOMSTR'] = "Linking $TARGET"
        

Export('env')

Progress(['-\r', '\\\r', '|\r', '/\r'], interval=5)

# die on unknown variables
unknown = vars.UnknownVariables()
if unknown:
        print "Unknown variables:", unknown.keys()
        Exit(1)

#generate help
Help(vars.GenerateHelpText(env))

for subdir in dirs:
	SConscript(
		subdir + os.sep + 'SConscript',
		duplicate = 0
		)
