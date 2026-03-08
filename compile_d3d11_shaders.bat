@echo off

if not exist data\shaders\compiled mkdir data\shaders\compiled

fxc /nologo /I "data/shaders" /T vs_5_0 /E vertex_main /Fo data/shaders/compiled/basic.vertex.fxc data/shaders/basic.hlsl
fxc /nologo /I "data/shaders" /T ps_5_0 /E pixel_main /Fo data/shaders/compiled/basic.pixel.fxc data/shaders/basic.hlsl

fxc /nologo /I "data/shaders" /T vs_5_0 /E vertex_main /Fo data/shaders/compiled/resolve.vertex.fxc data/shaders/resolve.hlsl
fxc /nologo /I "data/shaders" /T ps_5_0 /E pixel_main /Fo data/shaders/compiled/resolve.pixel.fxc data/shaders/resolve.hlsl

fxc /nologo /I "data/shaders" /T vs_5_0 /E vertex_main /Fo data/shaders/compiled/bloom.vertex.fxc data/shaders/bloom.hlsl
fxc /nologo /I "data/shaders" /T ps_5_0 /E pixel_main /Fo data/shaders/compiled/bloom.pixel.fxc data/shaders/bloom.hlsl

fxc /nologo /I "data/shaders" /T vs_5_0 /E vertex_main /Fo data/shaders/compiled/shadow.vertex.fxc data/shaders/shadow.hlsl
fxc /nologo /I "data/shaders" /T ps_5_0 /E pixel_main /Fo data/shaders/compiled/shadow.pixel.fxc data/shaders/shadow.hlsl
