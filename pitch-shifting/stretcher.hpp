#pragma once

// to get to know further rubberband source code details, wonder to participate with 
// rubberband-library visual studio project, but still has more glitch than meson build
// (should be the compiler options issue...)
#ifdef _WIN32
#if NDEBUG
// build by meson
#pragma comment(lib, "rubberband.lib")
#else
// build by visual studio project
#pragma comment(lib, "rubberband-library.lib")
#endif
#else
// unix-like for .a .o or .dylib
#pragma comment(lib, "librubberband.a")
#pragma comment(lib, "librubberband_objlib.a")
#endif

// for cross-platform using std::min/max on rubberband source code, consistent from stl instead of windows marco
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include "rubberband/RubberBandStretcher.h"
#include <iostream>
#pragma comment(lib, "sndfile.lib")
#include <sndfile.h>
#include <cmath>
#include <time.h>
#include <cstdlib>
#include <cstring>
#include <string>

#include <fstream>

#include <portaudio.h>
// for synchronization buffer between rubberband stretcher and portaudio callback
#include <mutex>
#include <condition_variable>
#include <deque>
#include "src/common/RingBuffer.h"
// for ChannelData struct
#include <src/finer/R3Stretcher.h>
// for all options to replace partial local variables
#include "parameters.h"

using std::cerr;
using std::endl;
using RubberBand::RubberBandStretcher;
using RubberBand::RingBuffer;
using std::string;

namespace PitchShifting {

enum SourceType {
    Unknown,
    AudioFile,
    AudioDevice,
};
struct SourceDesc {
    SourceType type = SourceType::Unknown;
    int index = -1;
    string desc = "";
    //TODO: or just given PaDeviceInfo...?
    int inputChannels = 0;
    int outputChannels = 0;
    int sampleRate = 0;
    // operands impl
    inline bool operator!() const { return type == SourceType::Unknown || index == -1; }
};

class Stretcher {
public:
    Stretcher(Parameters* parameters, int defBlockSize = 1024, int dbgLevel = 1);
    virtual ~Stretcher();
    // function can be private calls from stretcher itself which caller does not care at all
    // merge crispness with overwriting partial options which mainly works on R2(faster) engine
    int SetOptions(bool finer, bool realtime, int typewin, bool smoothing, bool formant,
        bool together, bool hipitch, bool lamination,
        int typethreading, int typetransient, int typedetector, int crispness = -1);
    // so far im not using time map...
    bool LoadTimeMap(std::string mapFile);
    // TODO: cannot find reference in macOS rubberbandstretcher
    //void SetKeyFrameMap() { if (pts && timeMap.size() > 0) pts->setKeyFrameMap(timeMap); };
    // redo or before set options to correct given parameters for rubberband
    bool LoadFreqMap(std::string mapFile, bool pitchToFreq);
    // clipping check during process frame
    void SetIgnoreClipping(bool ignore) { ignoreClipping = ignore; };

    // helper function to get SF_FORMAT_XXXX from file extension, \return 0 if not found
    int GetFileFormat(std::string extName);
    // list the local audio files to a list
    int ListLocalFiles(std::vector<SourceDesc>& files);
    // load input before create stretcher for time ratio and frames duration given to rubberband
    bool LoadInputFile(std::string fileName,
        int *pSampleRate, int *pChannels, int *pFormat, int64_t *pFramesCount,
        double ratio = 1.0, double duration = 0.0);
    // assign output wav file with given parameters
    bool SetOutputFile(std::string fileName,
        int sampleRate, int channels, int format);

    // (re)create the RubberBandStretcher, required settings from ctor given parameters, and input source description
    void Create();
    // works on input file, ignore in realtime mode
    void ExpectedInputDuration(size_t samples);
    void MaxProcessSize(size_t samples);
    double FormantScale(double scale = 0.0);

    // compare to stretcher::channels and block size for buffer allocation/reallocation
    void PrepareInputBuffer(int channels, int blocks, size_t reserves, int prevChannels);
    void PrepareOutputBuffer(int channels, int blocks, size_t reserves);

    // study loaded input file for stretcher (to pitch analyzing?), ignore in realtime mode 
    void StudyInputSound(/*int blockSize*/);
    // in realtime mode, process padding at begin to avoid fade in and get drop delay for output buffer
    int ProcessStartPad();
    // set dropping sample count for output which realtime mode has padding at begin
    void SetDropFrames(int frames) { dropFrames = frames; };
    // adjust rubberband pitch scale per process block, countIn will align to freqMap key and increase freqMap iterator
    void ApplyFreqMap(size_t countIn,/* int blockSize,*/ int *pAdjustedBlockSize = nullptr);
    
    // set input gain to audio signal, default 1.f
    void SetInputGain(float val) { inGain = val; };
    // set output gain for clipping manipulation, default 1.f
    void SetOutputGain(float val) { outGain = val; };
    // process given block of sound file, NOTE: high relavent to sndfile seeking position
    // return input frames have done for reading
    // TODO: w/o file input, we may not able to check isFinal or not for rubberband stretcher
    bool ProcessInputSound(/*int blockSize, */int *pFrame, size_t *pCountIn);
    // only care about available data on rubberband stretcher
    bool RetrieveAvailableData(size_t *pCountOut, bool isfinal = false);

    // helper function if using input/output sndfiles
    void CloseInputFile();
    void CloseOutputFile();

    // list audio devices via portaudio
    int ListAudioDevices(std::vector<SourceDesc>& devices);

    bool SetInputStream(int index, int *pSampleRate = nullptr, int *pChannels = nullptr);
    void StartInputStream() { if (inStream) Pa_StartStream(inStream); }
    void StopInputStream() { if (inStream) Pa_StopStream(inStream); }
    void CloseInputStream() { if (inStream) Pa_CloseStream(inStream); inStream = nullptr; }
    bool SetOutputStream(int index);
    void StartOutputStream() { if (outStream) Pa_StartStream(outStream); };
    void StopOutputStream() { if (outStream) Pa_StopStream(outStream); };
    void CloseOutputStream() { if (outStream) Pa_CloseStream(outStream); outStream = nullptr; };
    // DEBUG: original design for waiting audio stream to receive/send audio frames in portaudio callback, now use main loop instead
    void WaitStream(int timeout = 2000) { if (inStream || outStream) Pa_Sleep(timeout); };
    
    /* choosen source by set input stream/load input file */
    SourceDesc inSrcDesc;
    /* choosen source by set output stream/set output file */
    SourceDesc outSrcDesc;

    int GetDefBlockSize() { return defBlockSize; }
    int64_t totalFramesCount = 0; //isolated from main func
    size_t inputCount = 0; //isolated from main func
    size_t outputCount = 0; //isolated from main func
    // for status control(if run in the thread)
    bool stop = false;
    bool stopped = false;

    //DEBUG: try to use consistent size of single frame for display buffer, data just overwritten in next frame arrived
    float* inFrame;
    float* outFrame;

    //DEBUG: try pointer of std::shared_ptr<R3Stretcher::ChannelData>
    RubberBand::R3Stretcher::ChannelData* GetChannelData();

    //DEBUG: ensure rubberband library is compiled correctly
    std::string GetLibraryVersion();

    /* for getting formant data */
    enum FormantDataType : int {
        Cepstra, // fft size
        Envelope, // buf size
        Spare // buf size
    };
    int GetFormantFFTSize();
    void GetFormantData(FormantDataType type, int channel, int* fftSize, double** dataPtr, int* bufSize);
    
    /* for getting scale data */
    enum ScaleDataType : int {
        TimeDomain, // size will be fft size
        Real, // or amplitude
        Imaginary,
        Magnitude,
        Phase,
        AdvancedPhase,
        PreviousMagnitude,
        PendingKick,
        Accumulator
    };
    /* get numbers of FFT sizes for channel data scaling, given allocated fftSizes to get all size value */
    int GetChannelScaleSizes(int channel, int* fftSizes = nullptr);
    /* get each scales data ptr */
    void* GetChannelScaleData(ScaleDataType type, int channel, int fftSize, double** dataPtr, int* bufSize);

protected:
    // make sure deconstruction will be done
    void dispose();

    PaStreamCallback* debugCallback;

    const PaDeviceInfo* inInfo = nullptr;
    const PaDeviceInfo* outInfo = nullptr;

    PaStream* inStream;
    PaStream* outStream;

    static int inputAudioCallback(
        const void* inBuffer, void* outBuffer,
        unsigned long frames,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags flags,
        void *data);
    static int outputAudioCallback(
        const void* inBuffer, void* outBuffer,
        unsigned long frames,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags flags,
        void *data);

private:
    int debug;
    bool quiet;

    RubberBandStretcher *pts;
    RubberBandStretcher::Options options;
    
    // options from CLI or GUI from ctor to rubber band stretcher creation
    Parameters* param;

    std::map<size_t, size_t> timeMap;
    std::map<size_t, double> freqMap;
    std::map<size_t, double>::const_iterator freqMapItr;

    // default Transients(2)
    enum _t_transients {
        NoTransients,
        BandLimitedTransients,
        Transients
    } transients;

    // default Compound(0)
    enum _t_detector {
        CompoundDetector,
        PercussiveDetector,
        SoftDetector
    } detector;

    // sound file for input wav
    SNDFILE *sndfileIn;
    // input sound info
    SF_INFO sfinfoIn;
    // sound file for output wav
    SNDFILE *sndfileOut;
    // output sound info
    SF_INFO sfinfoOut;

    int defBlockSize = 1024;

    // for own input device, i need to check input amplitude
    float debugInMaxVal;
    // gain <-> ratio, http://www.sengpielaudio.com/calculator-FactorRatioLevelDecibel.htm
    // input power factor, default 1.f, (seems) use voltage ratio for audio float signal, pow(10.f, db / 20.f)?
    float inGain;

    // buffer for rubberband calculation
    float **cbuf;
    // buffer for input audio frame
    float *ibuf;
    // buffer for output audio frame
    float *obuf;

    // NOTE: due to use portaudio stream callback, buffer will be 1-dim within multi channels
    //       likes buffer[channels * block size]
    RingBuffer<float> *inBuffer;
    RingBuffer<float> *outBuffer;
    // addtional buffer size, almost 1s or assign by sample rate
    size_t reserveBuffer = 32768;
    // just want to see the buffer usage
    bool debugBuffer = true;
    time_t debugTimestampIn = time(nullptr);
    time_t debugTimestampOut = time(nullptr);
    //int debugBufTimerIn = 0;
    //int debugBufTimerOut = 0;

    // mutex for in/out ring buffer
    std::mutex inMutex;
    std::mutex outMutex;

	int outDelayFrames = 2000;

    int dropFrames;
    bool ignoreClipping;
    // decrease gain to avoid clipping for output process, default 1.f
    float outGain;
    // below minimal gain will mean dropping
    const float minGain = 0.75f;
};

} // namespace PitchShifting
