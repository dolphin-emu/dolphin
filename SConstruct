import os
import sys

# TODO: What is the current Dolphin version?
dolphin_version = '1.04'
Export('dolphin_version')

warnings = ' -Wall -Wwrite-strings -Wfloat-equal -Wshadow -Wpointer-arith -Wpacked -Wno-conversion'

nonactive_warnings = '-Wunreachable-code'

ccflags = '-g -O3 -fno-strict-aliasing -fPIC -msse2 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE' + warnings
#ccflags += ' -DLOGGING'

if sys.platform == 'darwin':         
	ccflags += ' -I/opt/local/include'

if False:
	ccflags += ' -fomit-frame-pointer'

include_paths = ["../../../Core/Common/Src",
                 "../../../Core/DiscIO/Src",
                 "../../../PluginSpecs",
                 "../../../",
                 "../../../Core/Core/Src",
                 "../../../Core/DebuggerWX/src",
                 "../../../../Externals/Bochs_disasm",
                 "../../../Core/VideoCommon/Src",
#                 "../../../Plugins/Plugin_VideoOGL/Src/Windows",
                 ]

builders = {}
if sys.platform == 'darwin':         
	from plistlib import writePlist
	def create_plist(target, source, env):
		properties = {}
		for src_node in source:
			properties.update(src_node.value)
		for dst_node in target:
			writePlist(properties, str(dst_node))
	builders['Plist'] = Builder(action = create_plist)

	dirs = ["Source/Core/Common/Src",
        	"Externals/Bochs_disasm",
	        "Source/Core/Core/Src",
       	 	"Source/Core/DiscIO/Src",
            "Source/Core/DebuggerWX/src",
            "Source/Core/VideoCommon/Src",
           "Source/Plugins/Plugin_VideoOGL/Src",
            "Source/Plugins/Plugin_DSP_NULL/Src",
#           "Source/Plugins/Plugin_DSP_LLE/Src",
            "Source/Plugins/Plugin_PadSimple/Src",
            "Source/Plugins/Plugin_nJoy_SDL/Src",
            "Source/Core/DolphinWX/src",
           ]
else:
	dirs = ["Source/Core/Common/Src",
        "Externals/Bochs_disasm",
        "Source/Core/Core/Src",
        "Source/Core/DiscIO/Src",
        "Source/Core/DebuggerWX/src",
        "Source/Core/VideoCommon/Src",
        "Source/Plugins/Plugin_VideoOGL/Src",
		"Source/Plugins/Plugin_DSP_NULL/Src",
#		"Source/Plugins/Plugin_DSP_LLE/Src",
		"Source/Plugins/Plugin_PadSimple/Src",
		"Source/Plugins/Plugin_nJoy_SDL/Src",
        "Source/Core/DolphinWX/src",
       ]


lib_paths = include_paths

env = Environment(CC="gcc", 
                  CXX="g++",
                  CCFLAGS=ccflags, 
                  CXXFLAGS=ccflags,
                  CPPPATH=include_paths, 
                  LIBPATH=lib_paths,
                  ENV={'PATH' : os.environ['PATH'],
                       'HOME' : os.environ['HOME']},
                  BUILDERS = builders,
                  )

Export('env')

builddir = "build"

for dir in dirs:
	SConscript(
		dir + os.sep + "SConscript",
#		build_dir = builddir + os.sep + dir,
		duplicate = 0
		)

