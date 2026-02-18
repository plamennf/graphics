@echo off

if not exist data\shaders\compiled mkdir data\shaders\compiled

fxc /nologo /I "data/shaders" /T vs_5_0 /E vertex_main /Fo data/shaders/compiled/basic.vertex.fxc data/shaders/basic.hlsl
fxc /nologo /I "data/shaders" /T ps_5_0 /E pixel_main /Fo data/shaders/compiled/basic.pixel.fxc data/shaders/basic.hlsl
