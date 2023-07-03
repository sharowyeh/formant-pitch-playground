# Hmm #

if i can build my own pitch shifting and formant adjustment software likes koigoe

# TD-PSOLA #

https://courses.engr.illinois.edu/ece420/sp2023/lab5/lab/#overlap-add-algorithm


# impl #

Rubberband: 
https://github.com/breakfastquay/rubberband

c++ implementation: 
https://github.com/terrykong/Phase-Vocoder.git

python implementation: 
https://github.com/sannawag/TD-PSOLA

JUCE vst plugin: 
https://forum.juce.com/t/realtime-rubberband-pitchshifter-working-example/46874/5
repo: 
https://github.com/jiemojiemo/rubberband_pitch_shift_plugin

# audio-processing.sln for windows env #

- NOTE: windows is not ideal dev env for rubberband project... lots of env pre-setting
- git pull rubberband into parent of this repo folder
- sndfile release files into this repo folder
  - https://github.com/libsndfile/libsndfile/releases
- murmur... hate to build dev env
```
-- formant-pitch-playground
|- formant-pitch-playground/audio-processing.sln
|- formant-pitch-playground/libsndfile-1.2.0/
|- formant-pitch-playground/libsndfile-1.2.0/win32/include
|- formant-pitch-playground/libsndfile-1.2.0/win64/include
|- rubberband/
|- rubberband/otherbuilds
|- ...
```

# rubberband dependencies(for windoiws env) #

UPDATE: using vcpkg to compiling rubberband with its dependencies in meson

- git pull vcpkg source from github
- add PATH with C:\path\to\vcpkg\vcpkg.exe
- install pkgconf in vcpkg, `>vcpkg install --triplet x64-windows pkgconf`
- meson.build add `project`'s `default options` with `'pkg_config_path=/path/to/vcpkg/installed/x64-windows/lib/pkgconfig'`
- set PKG_CONFIG=C:\path\to\vcpkg\installed\x64-windows\tools\pkgconf\pkgconf.exe
- install fftw, libsamplerate, etc via vcpkg
- `meson setup build --wipe -Dprefix=C:\path\to\cwd -Dextra_build_dirs=C:\path\to\vcpkg\installed\x64-windows\include`
- refer to
  - https://github.com/mesonbuild/meson/issues/3500#issuecomment-1236199562
  - https://github.com/mesonbuild/meson/issues/3500#issuecomment-1236378795

I believe it would be much easier in unix-like env... 

Got unresolve issues if force to add related paths to vcxproj properties and preprocessor definitions
TODO: may go checkout meson.build for these dependencies in visual studio in windows?

- fftw3
  - download precompiled dlls, use visual studio native command prompt generate .lib files
  - `> lib.exe /def:libfftw3-3.def`
  - `> lib.exe /machine:x64 /def:libfftw3-3.def`
  - vDSP vs FFTW (11 yrs ago) https://gist.github.com/admsyn/3452792

- sleef(dft)
  - build from source code, for windows -> cygwin
  - https://sleef.org/dft.xhtml

- intel IPP
  - integrated in oneapi https://www.intel.com/content/www/us/en/docs/ipp/developer-guide-oneapi/2022-2/finding-intel-ipp-on-your-system.html

# py version #

- locate at py folder, using python v3.10.12 for dev env on my macos m1
- py/requirements.txt

# Mac M1 version #
- refer to .vscode folder
- for rubberband functions, fetch github source code on parent folder for its profiler namespace(for logger or something)
- but still using homebrew installed rubberband libraries for compiling linker
- brew install: rubberband, libsndfile, portaudio
- for vscode c/c++ extension:
  - /opt/homebrew/include
  - /Library/Developer/CommandLineTools/xxxx
- for vscode compiling task:
  - -I/opt/homebrew/include
  - -I<path_to_rubberband_repo>/<for_profiler_src>
  - -L/opt/homebrew/lib
  - -lrubberband -lsndfile -lportaudio (for libxxxxx.dylib)
- for arm64 debugger tool, try lldb instead of gdb

- c macro for mac/m1: 
  - `> gcc -dM -E - < /dev/null | egrep -i 'os_|mac|apple'`
  - `#ifdef __arm64__`
  - `#include "TargetConditionals.h"` then use macro TARGET_xxx from obj-c example

- dylib linker and dev tools
  - https://stackoverflow.com/questions/52981210/linking-with-dylib-library-from-the-command-line-using-clang
  - otool
  - install_name_tool
  - for @rpath, @executable_path and etc...
  - `-Wl,-rpath,@executable_path`

# bela.io board #

for Bela dev board, can refer to NE10 library for data manipulation
https://projectne10.github.io/Ne10/doc

# from gpt #

## how to use ne10 library implement TD-POSLA for pitch and formant adjustment ##

To implement TD-POSLA (Time Domain Pitch-Synchronous OverLap and Add) for pitch and formant adjustment using the NE10 library, you would need to follow these general steps:

Setup NE10 library: Start by downloading and setting up the NE10 library in your project. Make sure you have the necessary dependencies and header files properly included.

Preprocessing: Read the audio signal that you want to modify for pitch and formant adjustment. Perform any necessary preprocessing steps, such as resampling or windowing, to prepare the signal for processing.

Pitch analysis: Use a pitch analysis algorithm (e.g., autocorrelation, cepstral analysis, or harmonic product spectrum) to estimate the pitch period or fundamental frequency of the signal. This step helps in determining the pitch adjustment factor.

Formant analysis: Employ a formant analysis algorithm (e.g., linear predictive coding) to extract the formant frequencies and bandwidths from the signal. This step aids in determining the formant adjustment factors.

Time Domain Pitch-Synchronous OverLap and Add (TD-POSLA): Apply the TD-POSLA algorithm to modify the pitch and formants of the signal. The TD-POSLA algorithm involves the following steps:

a. Determine the desired pitch adjustment factor based on the target pitch and the estimated pitch from step 3.

b. Determine the desired formant adjustment factors based on the target formant frequencies and bandwidths and the estimated formant information from step 4.

c. Divide the input signal into overlapping frames.

d. For each frame:

i. Adjust the pitch of the frame by modifying its length according to the pitch adjustment factor.

ii. Adjust the formants of the frame by modifying the frequencies and bandwidths based on the formant adjustment factors.

iii. Overlap and add the modified frame to reconstruct the output signal.

e. Repeat step 5d for each frame until the entire input signal is processed.

Post-processing: Apply any necessary post-processing steps, such as windowing or normalization, to the modified signal to ensure a smooth transition between frames and an appropriate amplitude range.

Output: Save the modified signal to a file or use it for further processing or analysis.

It's important to note that implementing TD-POSLA for pitch and formant adjustment using the NE10 library requires a good understanding of digital signal processing concepts, including pitch and formant analysis techniques. Additionally, you may need to consult the NE10 library documentation and examples to understand the specific functions and data structures available for audio signal processing in the library.

