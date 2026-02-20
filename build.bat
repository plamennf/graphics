@echo off

call compile_d3d11_shaders.bat

vendor\premake\premake5.exe vs2026

msbuild graphics.slnx -v:m -p:Configuration=Debug
REM msbuild graphics.slnx -v:m -p:Configuration=Release
REM msbuild graphics.slnx -v:m -p:Configuration=Dist
