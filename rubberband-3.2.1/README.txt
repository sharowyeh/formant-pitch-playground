Compiled binaries incase I got lazy disease

Windows environment:
Requires vcpkg and with installed tool pkgconf to include dependences, fftw3, samplerate and sndfile
NOTE: some reason I need to manipulate rubberband internal data, still better programming in visual studio project
meson-cl: using meson configurate builds for CL.exe compiling rubberband.lib
msvc: capable compile via visual studio vcxproj with includeded vcpkg paths

Mac environment:
homebrew is best friend, then refer to .vscode c++ configs