::: place rubberband repo at the parent directory
::: make sure required deps installed via vcpkg
::: set pkg config path to pkgconf.exe and add to meson.build project options
cd /d ..\rubberband

::: x64 release
mkdir build-x64-rel

meson setup build-x64-rel --wipe -Dprefix=C:\Users\minamo\projects\tts\rubberband -Dextra_include_dirs=C:\Users\minamo\projects\vcpkg\installed\x64-windows\include -Dextra_lib_dirs=C:\Users\minamo\projects\vcpkg\installed\x64-windows\lib -Dfft=fftw -Dresampler=libsamplerate -Dbuildtype=release 

::: x64 debug
mkdir build-x64-dbg

meson setup build-x64-dbg --wipe -Dprefix=C:\Users\minamo\projects\tts\rubberband -Dextra_include_dirs=C:\Users\minamo\projects\vcpkg\installed\x64-windows\include -Dextra_lib_dirs=C:\Users\minamo\projects\vcpkg\installed\x64-windows\debug\lib -Dfft=fftw -Dresampler=libsamplerate -Dbuildtype=debug 


::: ninja -C build-x64-rel
ninja -C build-x64-dbg