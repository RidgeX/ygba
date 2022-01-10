- Ubuntu

```sh
$ sudo apt install build-essential cmake libfreetype-dev libsdl2-dev
$ ./build-unix.sh
```

- Fedora

```sh
$ sudo dnf install make gcc gcc-c++ cmake freetype-devel SDL2-devel
$ ./build-unix.sh
```

- Windows (MSYS2)

```cmd
> set MSYS_HOME=C:\msys64
> set MINGW_HOME=C:\msys64\mingw64
> set PATH=%PATH%;%MINGW_HOME%\bin;%MSYS_HOME%\usr\bin
> pacman -S --needed base-devel pactoys
> pacboy -S --needed toolchain:x cmake:x freetype:x SDL2:x
> build-msys.bat
```

- Windows (vcpkg)

```cmd
> set VCPKG_ROOT=C:\vcpkg
> cd /d C:\
> git clone https://github.com/microsoft/vcpkg.git
> cd vcpkg
> bootstrap-vcpkg.bat
> vcpkg install freetype sdl2 --triplet x64-windows
> vcpkg integrate install
> build-vs2022.bat
```
