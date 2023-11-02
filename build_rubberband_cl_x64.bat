ECHO ensure using x64 or x86_64 command prompt for visual studio to run this batch in windows environment
::: place rubberband repo at the parent directory
::: make sure required deps installed via vcpkg
::: set pkg config path to pkgconf.exe and add to meson.build project options
cd /d ..\rubberband

ECHO %VCPKG_ROOT%

if [%VCPKG_ROOT%] == [] ECHO VCPKG_ROOT must be assigned && exit /b

::: check vcpkg and pkgconfig related environment for meson.build usage
::: ensure the pkgconf is from vcpkg instead of cygwin
SET PKG_CONFIG=%VCPKG_ROOT%\installed\x64-windows\tools\pkgconf\pkgconf.exe
ECHO %PKG_CONFIG%

::: x64 release
mkdir build-x64-rel

meson setup build-x64-rel --wipe -Dprefix=%~dp0 -Dpkg_config_path=%VCPKG_ROOT%\installed\x64-windows\lib\pkgconfig -Dextra_include_dirs=%VCPKG_ROOT%\installed\x64-windows\include -Dextra_lib_dirs=%VCPKG_ROOT%\installed\x64-windows\lib -Dfft=fftw -Dresampler=libsamplerate -Dbuildtype=release 

::: x64 debug
mkdir build-x64-dbg

meson setup build-x64-dbg --wipe -Dprefix=%~dp0 -Dpkg_config_path=%VCPKG_ROOT%\installed\x64-windows\lib\pkgconfig -Dextra_include_dirs=%VCPKG_ROOT%\installed\x64-windows\include -Dextra_lib_dirs=%VCPKG_ROOT%\installed\x64-windows\debug\lib -Dfft=fftw -Dresampler=libsamplerate -Dbuildtype=debug 


::: ninja -C build-x64-rel
ninja -C build-x64-dbg