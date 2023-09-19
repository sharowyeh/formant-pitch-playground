#pragma once

// to get to know further rubberband source code details, wonder to participate with 
// rubberband-library visual studio project, but still has more glitch than meson build
// (should be the compiler options issue...)
#if NDEBUG
// build by meson
#pragma comment(lib, "rubberband.lib")
#else
// build by visual studio project
#pragma comment(lib, "rubberband-library.lib")
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
    Stretcher(int defBlockSize = 1024, int dbgLevel = 1);
    virtual ~Stretcher();

    int SetOptions(bool finer, bool realtime, int typewin, bool smoothing, bool formant,
        bool together, bool hipitch, bool lamination,
        int typethreading, int typetransient, int typedetector);
    // so far im not using time map...
    bool LoadTimeMap(std::string mapFile);
    void SetKeyFrameMap() { if (pts && timeMap.size() > 0) pts->setKeyFrameMap(timeMap); };
    // redo or before set options to correct given parameters for rubberband
    bool LoadFreqMap(std::string mapFile, bool pitchToFreq);
    // mainly works on R2(faster) engine, redo or before set options to correct given parameters for rubberband
    void SetCrispness(int crispness = -1, 
        int *ptypewin = nullptr, bool *plamination = nullptr,
        int *ptypetransient = nullptr, int *ptypedetector = nullptr);
    // clipping check during process frame
    void SetIgnoreClipping(bool ignore) { ignoreClipping = ignore; };

    // helper function to get SF_FORMAT_XXXX from file extension
    int GetFileFormat(std::string extName);
    // load input before create stretcher for time ratio and frames duration given to rubberband
    bool LoadInputFile(std::string fileName,
        int *pSampleRate, int *pChannels, int *pFormat, int64_t *pFramesCount,
        double ratio = 1.0, double duration = 0.0);
    // assign output wav file with given parameters
    bool SetOutputFile(std::string fileName,
        int sampleRate, int channels, int format);

    // basically given ctor params to fullfill RubberBandStretcher
    void Create(size_t sampleRate, int channels, int options, double timeRatio, double pitchScale);
    // works on input file, ignore in realtime mode
    void ExpectedInputDuration(size_t samples);
    void MaxProcessSize(size_t samples);
    double FormantScale(double scale = 0.0);

    // compare to stretcher::channels and block size for buffer allocation/reallocation
    void PrepareInputBuffer(int channels, int blocks, size_t reserves);
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
    void CloseFiles();

    // list audio devices via portaudio
    int ListAudioDevices(std::vector<SourceDesc>& devices);
    PaStreamCallback *debugCallback;

    const PaDeviceInfo* inInfo = nullptr;
    const PaDeviceInfo* outInfo = nullptr;

    bool SetInputStream(int index, int *pSampleRate = nullptr, int *pChannels = nullptr);
    void StartInputStream() { if (inStream) Pa_StartStream(inStream); }
    void StopInputStream() { if (inStream) Pa_StopStream(inStream); }
    bool SetOutputStream(int index);
    void StartOutputStream() { if (outStream) Pa_StartStream(outStream); };
    void StopOutputStream() { if (outStream) Pa_StopStream(outStream); };
    // DEBUG: original design for waiting audio stream to receive/send audio frames in portaudio callback, now use main loop instead
    void WaitStream(int timeout = 2000) { if (inStream || outStream) Pa_Sleep(timeout); };
    
    //DEBUG: signal frames for waveform display, should be the same with in/outBuffer, so far i give it 1 sec buffer size
    //TODO: i think these GUI buffers are better initilize from GUI, and given to this class to fillout
    //TODO: can use atomic flag for GUI prevent GUI reading buffer to block buffer appending...? not good
    RingBuffer<float>* inFrames;
    RingBuffer<float>* outFrames;

    //DEBUG: try pointer of std::vector<std::shared_ptr<R3Stretcher::ChannelData>>
    void* GetChannelData();
    int GetFormantFFTSize();

protected:
    // make sure deconstruction will be done
    void dispose();

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
    
    // estimate frame ratio from input sound for output, default 1.0
    double ratio;
    // estimate output sound file duration, default unset(0.0)
    double duration;
    // percentage, default 1.0
    double frequencyshift;
    bool realtime;
    int threading;
    bool lamination;
    bool longwin;
    bool shortwin;
    bool smoothing;
    bool hqpitch;
    bool formant;
    bool together;
    // default -1
    int crispness;
    bool faster;
    bool finer;

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

    int inputChannels;
    int outputChannels;
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
    int debugBufTimerIn = 0;
    int debugBufTimerOut = 0;
    // TODO: poor perf in windows env allocate/reallocate memory
    std::mutex inMutex;
    //std::deque<float*> inChunks;
    std::mutex outMutex;
    //int chunkWrite = 0;
    //std::deque<float*> outChunks;
    // delay for available blocks to audio output, about 0.25s:
    // Mac default vDSP quite performanced that can reduce to 4096 frames including rubberband dropped
    // Windows env using fftw and libsamplerate also can recude less than 2000 frames
	int outDelayFrames = 2000;

    int dropFrames;
    bool ignoreClipping;
    // decrease gain to avoid clipping for output process, default 1.f
    float outGain;
    // below minimal gain will mean dropping
    const float minGain = 0.75f;
};

} // namespace PitchShifting
