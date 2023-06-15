# Hmm #

if i can build my own pitch shifting and formant adjustment software likes koigoe

# TD-PSOLA #

https://courses.engr.illinois.edu/ece420/sp2023/lab5/lab/#overlap-add-algorithm


# impl #

Rubberband
https://github.com/breakfastquay/rubberband

c++ implementation:
https://github.com/terrykong/Phase-Vocoder.git

python implementation:
https://github.com/sannawag/TD-PSOLA


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

