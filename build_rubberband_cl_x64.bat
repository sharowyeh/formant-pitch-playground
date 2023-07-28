::: place rubberband repo at the parent directory
::: make sure required deps installed via vcpkg
::: set pkg config path to pkgconf.exe and add to meson.build project options
cd /d ..\rubberband

::: check vcpkg and pkgconfig related environment for meson.build usage
SET PKG_CONFIG=C:\Users\minamo\projects\vcpkg\installed\x64-windows\tools\pkgconf\pkgconf.exe
ECHO %PKG_CONFIG%
ECHO %VCPKG_ROOT%

::: x64 release
mkdir build-x64-rel

meson setup build-x64-rel --wipe -Dprefix=C:\Users\minamo\projects\tts\rubberband -Dextra_include_dirs=C:\Users\minamo\projects\vcpkg\installed\x64-windows\include -Dextra_lib_dirs=C:\Users\minamo\projects\vcpkg\installed\x64-windows\lib -Dfft=fftw -Dresampler=libsamplerate -Dbuildtype=release 

::: x64 debug
mkdir build-x64-dbg

meson setup build-x64-dbg --wipe -Dprefix=C:\Users\minamo\projects\tts\rubberband -Dextra_include_dirs=C:\Users\minamo\projects\vcpkg\installed\x64-windows\include -Dextra_lib_dirs=C:\Users\minamo\projects\vcpkg\installed\x64-windows\debug\lib -Dfft=fftw -Dresampler=libsamplerate -Dbuildtype=debug 


::: ninja -C build-x64-rel
ninja -C build-x64-dbg