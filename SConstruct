import os

ccflags = '-g -O2 -msse2 -Wall -D_DEBUG -DLOGGING -D_FILE_OFFSET_BITS=64 D_LARGEFILE_SOURCE'

if False:
	ccflags += ' -fomit-frame-pointer'

include_paths = ["../../../Core/Common/Src",
                 "../../../Core/DiscIO/Src",
                 "../../../PluginSpecs",
                 "../../../",
                 "../../../Core/Core/Src",
                 "../../../Core/DebuggerWX/src",
                 "../../../../Externals/Bochs_disasm",
#                 "../../../Plugins/Plugin_VideoOGL/Src/Windows",
                 ]

dirs = ["Source/Core/Common/Src",
        "Externals/Bochs_disasm",
        "Source/Core/Core/Src",
        "Source/Core/DiscIO/Src",
        "Source/Core/DebuggerWX/src",
        "Source/Plugins/Plugin_VideoOGL/Src",
		"Source/Plugins/Plugin_DSP/Src",
		"Source/Plugins/Plugin_DSP_LLE/Src",
		"Source/Plugins/Plugin_PadSimple/Src",
        "Source/Core/DolphinWX/src",
       ]

lib_paths = include_paths

env = Environment(CC="gcc-4.2", 
                  CXX="g++-4.2",
                  CCFLAGS=ccflags, 
                  CXXFLAGS=ccflags,
                  CPPPATH=include_paths, 
                  LIBPATH=lib_paths,
                  ENV={'PATH' : os.environ['PATH'],
                       'HOME' : os.environ['HOME']})

Export('env')

builddir = "build"

for dir in dirs:
	SConscript(
		dir + os.sep + "SConscript",
#		build_dir = builddir + os.sep + dir,
		duplicate = 0
		)

