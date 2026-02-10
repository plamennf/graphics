workspace "graphics"
	architecture "x86_64"
	startproject "sandbox"

	configurations {
		"Debug",
		"Release",
		"Dist"
	}

project "corelib"
    kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"
    multiprocessorcompile "on"

    targetdir ("%{wks.location}/build/%{cfg.buildcfg}")
	objdir ("%{wks.location}/build-int/%{cfg.buildcfg}/%{prj.name}")

	pchheader "corelib.h"
	pchsource "src/corelib/make_pch.cpp"

    files {
        "src/corelib/**.h",
        "src/corelib/**.cpp",
    }

    defines {
		"_CRT_SECURE_NO_WARNINGS"
	}

    filter "system:windows"
		systemversion "latest"

    filter "configurations:Debug"
		defines "BUILD_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "BUILD_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "BUILD_DIST"
		runtime "Release"
		optimize "on"

project "sandbox"
    kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"
    multiprocessorcompile "on"

    targetdir ("%{wks.location}/build/%{cfg.buildcfg}")
	objdir ("%{wks.location}/build-int/%{cfg.buildcfg}/%{prj.name}")

	pchheader "pch.h"
	pchsource "src/sandbox/make_pch.cpp"

    includedirs {
        "%{wks.location}/src"
    }

    links {
         "corelib",
    }

    files {
        "src/sandbox/**.h",
        "src/sandbox/**.cpp",
    }

    defines {
		"_CRT_SECURE_NO_WARNINGS"
	}

    filter "system:windows"
		systemversion "latest"

        links {
            "winmm.lib",
            "d3d12.lib",
            "dxgi.lib",
            "d3dcompiler.lib",
        }

    filter "configurations:Debug"
		defines "BUILD_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "BUILD_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "BUILD_DIST"
		runtime "Release"
		optimize "on"
