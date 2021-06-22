@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -G "Visual Studio 16 2019" -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build
msbuild build\ALL_BUILD.vcxproj -p:Configuration=RelWithDebInfo
