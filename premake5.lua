project "IsoSpec++"
	kind "StaticLib"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"unity-build.cpp"
	}

	includedirs
	{
		"include/IsoSpec++"
	}
	
	filter "system:windows"
	systemversion "latest"
	cppdialect "C++17"
	staticruntime "on"

	
	
	configurations { "Debug", "Release", "RelWithDebInfo", "MinSizeRel" }

	filter "configurations:MinSizeRel"
    	optimize "Size"
	
	
	filter "configurations:Debug"
    	symbols "On"
    	defines { "-ggdb3" }

	filter "configurations:Release"
    	optimize "On"
    	defines { "-DQT_NO_DEBUG_OUTPUT" }

	filter "configurations:RelWithDebInfo"
    	optimize "On"
    	symbols "On"
	
	
	
	
	
	
	


