#!/usr/bin/env bash
cmake -G "Unix Makefiles" -DUSE_VCPKG=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo -B build
make -C build
