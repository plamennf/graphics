@echo off

call compile_d3d11_shaders.bat

vendor\premake\premake5.exe vs2026

REM msbuild fps.slnx -v:m -p:Configuration=Debug
msbuild fps.slnx -v:m -p:Configuration=Release
REM msbuild fps.slnx -v:m -p:Configuration=Dist
