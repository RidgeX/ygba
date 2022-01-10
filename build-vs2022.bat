@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build
msbuild build\ALL_BUILD.vcxproj -p:Configuration=RelWithDebInfo
