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

using std::cerr;
using std::endl;
using RubberBand::RubberBandStretcher;

namespace PitchShifting {

class Stretcher {
public:
    Stretcher(int defBlockSize = 1024, int dbgLevel = 1);

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
    void Create(size_t sampleRate, size_t channels, int options, double timeRatio, double pitchScale);
    // works on input file, ignore in realtime mode
    void ExpectedInputDuration(size_t samples);
    void MaxProcessSize(size_t samples);
    double FormantScale(double scale = 0.0);

    // compare to stretcher::channels and block size for buffer allocation/reallocation
    void PrepareBuffer();
    // study loaded input file for stretcher (to pitch analyzing?), ignore in realtime mode 
    void StudyInputSound(/*int blockSize*/);
    // in realtime mode, process padding at begin to avoid fade in and get drop delay for output buffer
    void ProcessStartPad(int *pToDrop);
    // adjust rubberband pitch scale per process block, countIn will align to freqMap key and increase freqMap iterator
    void ApplyFreqMap(size_t countIn,/* int blockSize,*/ int *pAdjustedBlockSize = nullptr);
    
    // process given block of sound file, NOTE: high relavent to sndfile seeking position
    // TODO: we may need change return parameter to class scoped variable instead of caller?
    // pFrame represent current process input frame position to ensure is final for rubberband process() call
    bool ProcessInputSound(/*int blockSize, */int *pToDrop, int *pFrame,
        size_t *pCountIn, size_t *pCountOut, float gain = 1.f);
    // only care about available data on rubberband stretcher
    bool RetrieveAvailableData(size_t *pCountOut, float gain = 1.f);

    // helper function if using input/output sndfiles
    void CloseFiles();
protected:
    virtual ~Stretcher();
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

    bool ignoreClipping;

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
    const int defBlockSize = 1024;
    // buffer reallocation depends on channels and block size, reallocation via input/output changes
    bool reallocInBuffer;
    bool reallocOutBuffer;
    // temperary flag for cbuf dispose, align to inputChannels
    int cbufLen;
    // buffer for rubberband calculation
    float **cbuf;
    // buffer for input audio frame
    float *ibuf;
    // buffer for output audio frame
    float *obuf;
};

} // namespace PitchShifting
