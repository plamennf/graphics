@echo off

glslangvalidator -V -g -o data/shaders/compiled/test.vert.spv data/shaders/test.vert
glslangvalidator -V -g -o data/shaders/compiled/test.frag.spv data/shaders/test.frag

vendor\premake\premake5.exe vs2026

msbuild graphics.slnx -v:m -p:Configuration=Debug
