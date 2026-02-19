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

    includedirs {
        "external/include"
    }

    files {
        "src/corelib/**.h",
        "src/corelib/**.cpp",
        "external/src/imgui/**.cpp",
    }

    defines {
		"_CRT_SECURE_NO_WARNINGS",
        "ENABLE_IMGUI",
	}

    filter "files:external/src/imgui/**.cpp"
	    enablepch "off"

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
        "%{wks.location}/src",
        "%{wks.location}/external/include",
    }

    libdirs {
        "%{wks.location}/external/lib"
    }

    links {
         "corelib",
    }

    files {
        "src/sandbox/**.h",
        "src/sandbox/**.cpp",
        "extenral/src/imgui/**.cpp",
    }

    defines {
		"_CRT_SECURE_NO_WARNINGS",
        "ENABLE_IMGUI",
	}

    filter "files:external/src/imgui/**.cpp"
	    enablepch "off"

    filter "system:windows"
		systemversion "latest"

        defines { "RENDER_D3D11" }

        links {
            "winmm.lib",
            "d3d11.lib",
            "opengl32.lib",
            "freetype.lib",
        }

    filter "configurations:Debug"
		defines "BUILD_DEBUG"
		runtime "Debug"
		symbols "on"

        libdirs {
            "%{wks.location}/external/lib/Debug"
        }

	filter "configurations:Release"
		defines "BUILD_RELEASE"
		runtime "Release"
		optimize "on"

        libdirs {
            "%{wks.location}/external/lib/Release"
        }

	filter "configurations:Dist"
		defines "BUILD_DIST"
		runtime "Release"
		optimize "on"

        libdirs {
            "%{wks.location}/external/lib/Release"
        }
