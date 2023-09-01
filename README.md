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
|- portaudio/
|- portaudio/build/msvc
|- vcpkg/installed/x64-windows/
|- vcpkg/installed/x64-windows/include
|- vcpkg/installed/x64-windows/lib
|- ...
```

# rubberband r3 stretcher trace #
## stretcher -> process() given buf[c][i] 2d array ##
- check if inputs need resampling, and loop samples length to write m_channelData(cd) -> inbuf 
  - pre-resampling given inputs if smaller pitch scale(higher pitch) and option set quality
    - (higher pitch will have less sampling than origin)
  - loop inputs with index to copy to m_channelAssembly.input[c]
  - write to ring buffer m_channelData[c]->inbuf
- consume()
  - apply post-resampling given in hop size to calculate out hop size via calculateSingle()
  - until m_channelData cd->inbuf is enough for in hop fft window size via getWindowSourceSize()
  - analysisChannel()
    - read cd->inbuf to cd->windowSource
    - multiply unwindowed input frames to each cd->scales time domain data, except classification scale
      - TODO: check configuration for longest/shortest/classification fft sizes
      - graph for scales time domain dataset with longest fft size:
      ```
      FFT scales:                                               longest FFT size
      scale1       |----------------------|--------------|--------------|
                                 scale size        offset1
      scale2       |------------------------------|----------|----------|
                                            scale size    offset2
      windowSource |----------|----|------------------------------------>
                 buf*   +offset2  +offset1
      scale timeDomain:
      data1                        |---------------------|
      (multiply buf+offset1)
      data2                   |------------------------------|
      (multiply buf+offset2)
      ```
    - classification scale uses given inhop, and own cd->readahead(begins from buf+offset+inhop) instead of scale timeDomain data
    - fft shift, fft forward (to scale's timeDomain or classify readahead) and catesian-polar convert to each scales
      - fft shift swaps second half and first half time domain signals data
      - given time domain data calls fft real to complex to get real (scale->rea) and imaginary (scale->imag) data
      - convert output cartesian(x,y) data to polar(r,theta) data
      - normalize scale->mag by fft size
    - analyseFormant() and adjustFormant()
      - given normalized scale->mag calls fft complex to real to get cd->formant->cepstra
        - cepstram: normalize freq domain(likes dB and reduce noise), than apply ifft transform
        - eg, Mei-Frequency Cepstrum for speech recognition
        - http://disp.ee.ntu.edu.tw/meeting/%E5%86%BC%E9%81%94/termPaper_R98942120.pdf
      - cepstra cutoff by sample rate / 650, and normalize by fft size
      - given cepstra data calls fft read to complex to get output formant->envelope and formant->spara
      - exp then sqrt first half of formant->envelope (bin count, = fft size/2+1)
      - calculate target factor to each scales by formant fft size, target factor= formant fft size/scale fft size
      - apply formant by calculated source factor, source factor= target factor/formant scale (note pitch shifting also affact formant scale)
      - TODO: check configuration.fftBandLimits
      - based on cd->scales->fft size is band fft size, get source and target envelope value at(i * factor)
      - multiply magnitue scale->mag by ratio (source/target envelope)
    - use classification scale to get bin segmentation, copy next cd->classification to cd->scale->mag
      - also record to cd->prevSegmentation, current and the next
    - update all fft parameters to guide via updateGuidance TODO: check guide class
    - analysis channel end
 
## stretcher -> available() -> retrieve() ##
- TODO:

# rubberband dependencies(for windoiws env) #
For windows env, rubberband must be rebuild with 3rd party FFT for better performence results

UPDATE: using vcpkg to compiling rubberband with its dependencies in meson, DO NOT use rubberband-library visual studio project

- git pull vcpkg source from github
- add PATH with C:\path\to\vcpkg\vcpkg.exe (for cli/script usage)
- add VCPKG_ROOT with C:\path\to\vcpkg (for visual studio/cmake project dependency)
- install pkgconf in vcpkg, `>vcpkg install --triplet x64-windows pkgconf`
- meson.build add `project`'s `default options` with `'pkg_config_path=/path/to/vcpkg/installed/x64-windows/lib/pkgconfig'`
- set PKG_CONFIG=C:\path\to\vcpkg\installed\x64-windows\tools\pkgconf\pkgconf.exe
- refer to
  - https://github.com/mesonbuild/meson/issues/3500#issuecomment-1236199562
  - https://github.com/mesonbuild/meson/issues/3500#issuecomment-1236378795
- install fftw, libsamplerate, etc via vcpkg
- `meson setup build --wipe -Dprefix=C:\path\to\cwd -Dextra_include_dirs=C:\path\to\vcpkg\installed\x64-windows\include -Dextra_lib_dirs=C:\path\to\vcpkg\installed\x64-windows\lib`
- `-Dfft=fftw -Dresampler=libsamplerate`
- meson.build has been set NOMINMAX to ignore windows.h default min/max macro, so need stl lib for min/max(i choose rubberband\RubberBandStretcher.h)
  ```
  #ifdef _MSC_VER
  #include <algorithm>
  #endif
  ```

I believe it would be much easier in unix-like env... 

(not recommonded) download and compile manually as below:
(add related paths to vcxproj properties and preprocessor definitions, got unresolve issues)

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

