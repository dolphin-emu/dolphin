solution "enet"
	configurations { "Debug", "Release" }
	platforms { "x32", "x64" }

	project "enet_static"
		kind "StaticLib"
		language "C"
		
		files { "*.c" }
		
		includedirs { "include/" }
		
		configuration "Debug"
			targetsuffix "d"
			
			defines({ "DEBUG" })

			flags { "Symbols" }
		
		configuration "Release"
			defines({ "NDEBUG" })

			flags { "Optimize" }
			
		configuration { "Debug", "x64" }
			targetsuffix "64d"
			
		configuration { "Release", "x64" }
			targetsuffix "64"

	project "enet"
		kind "SharedLib"
		language "C"
		
		files { "*.c" }
		
		includedirs { "include/" }
		
		defines({"ENET_DLL=1" })
		
		configuration "Debug"
			targetsuffix "d"
			
			defines({ "DEBUG" })

			flags { "Symbols" }
		
		configuration "Release"
			defines({ "NDEBUG" })

			flags { "Optimize" }
			
		configuration { "Debug", "x64" }
			targetsuffix "64d"
			
		configuration { "Release", "x64" }
			targetsuffix "64"

		