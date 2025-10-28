-- premake5.lua
workspace "WalnutApp"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "Caustic"

   -- Workspace-wide build options for MSVC
   filter "system:windows"
      buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

include "Build-Walnut-External.lua"
include "Caustic/Build-Walnut-App.lua"