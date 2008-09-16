import os
import sys

# TODO: What is the current Dolphin version?
dolphin_version = '1.04'
Export('dolphin_version')

warnings = [
	'all',
	'write-strings',
	#'float-equal',
	'shadow',
	'pointer-arith',
	'packed',
	'no-conversion',
#	'error',
	#'unreachable-code',
	]
compileFlags = [
#	'-g',
	'-O3',
	'-fno-strict-aliasing',
	'-msse2',
	'-D_FILE_OFFSET_BITS=64',
	'-D_LARGEFILE_SOURCE',
	'-DGCC_HASCLASSVISIBILITY',
	'-fvisibility=hidden',
	]
compileFlags += [ '-W' + warning for warning in warnings ]
#compileFlags += [ '-DLOGGING' ]
#compileFlags += [ '-fomit-frame-pointer' ]
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
	"Source/Core/Common/Src",
	"Externals/Bochs_disasm",
        "Externals/LZO",
	"Source/Core/Core/Src",
	"Source/Core/DiscIO/Src",
	"Source/Core/DebuggerWX/src",
	"Source/Core/VideoCommon/Src",
	"Source/Plugins/Plugin_VideoOGL/Src",
	"Source/Plugins/Plugin_DSP_HLE/Src",
        "Source/Plugins/Plugin_DSP_LLE/Src",
	"Source/Plugins/Plugin_PadSimple/Src",
	"Source/Plugins/Plugin_nJoy_SDL/Src",
	"Source/Plugins/Plugin_Wiimote_Test/Src",
	"Source/Core/DolphinWX/Src",
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

env = Environment(
	CC = "gcc",
	CXX = "g++",
	CCFLAGS = ' '.join(compileFlags),
	CXXFLAGS = ' '.join(compileFlags + [ '-fvisibility-inlines-hidden' ]),
	CPPPATH = include_paths,
	LIBPATH = lib_paths,
	ENV = {
		'PATH' : os.environ['PATH'],
		'HOME' : os.environ['HOME']
		},
	BUILDERS = builders,
	)

Export('env')

for subdir in dirs:
	SConscript(
		subdir + os.sep + 'SConscript',
		duplicate = 0
		)
