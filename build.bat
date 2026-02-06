@echo off

vendor\premake\premake5.exe vs2026

msbuild graphics.sln -v:m -p:Configuration=Debug
