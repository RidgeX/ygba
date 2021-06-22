@echo off
cmake -G "MSYS Makefiles" -DUSE_VCPKG=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build
make -C build
