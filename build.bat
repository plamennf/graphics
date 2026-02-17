@echo off

vendor\premake\premake5.exe vs2026

msbuild graphics.slnx -v:m -p:Configuration=Debug
