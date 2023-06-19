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
    RubberBandStretcher *pts;
    RubberBandStretcher::Options options = 0;
    
    // estimate frame ratio from input sound for output
    double ratio = 1.0;
    // estimate output sound file duration
    double duration = 0.0;
    // semitones
    double pitchshift = 0.0;
    // percentage
    double frequencyshift = 1.0;
    int debug = 0;
    bool realtime = false;
    // precise became the default in v1.6 and loose was removed in v3.0
    bool precisiongiven = false;
    int threading = 0;
    bool lamination = true;
    bool longwin = false;
    bool shortwin = false;
    bool smoothing = false;
    bool hqpitch = false;
    bool formant = false;
    bool together = false;
    bool crispchanged = false;
    int crispness = -1;
    bool faster = false;
    bool finer = false;
    bool help = false;
    bool fullHelp = false;
    bool version = false;
    bool quiet = false;

    std::map<size_t, size_t> timeMap;
    std::map<size_t, double> freqMap;
    std::map<size_t, double>::const_iterator freqMapItr = freqMap.begin();

    enum _t_transients {
        NoTransients,
        BandLimitedTransients,
        Transients
    } transients = Transients;

    enum _t_detector {
        CompoundDetector,
        PercussiveDetector,
        SoftDetector
    } detector = CompoundDetector;

    bool ignoreClipping = false;

    // sound file for input wav
    SNDFILE *sndfileIn = nullptr;
    // input sound info
    SF_INFO sfinfoIn = { 0 };
    // sound file for output wav
    SNDFILE *sndfileOut = nullptr;
    // output sound info
    SF_INFO sfinfoOut = { 0 };
    // output file extension to estimate output sound format

    int inputChannels = 0;
    int outputChannels = 0;
    int defBlockSize = 1024;
    // buffer reallocation depends on channels and block size, reallocation via input/output changes
    bool reallocInBuffer = false;
    bool reallocOutBuffer = false;
    // temperary flag for cbuf dispose
    int cbufLen = 0;
    // buffer for rubberband calculation
    float **cbuf = nullptr;
    // buffer for input audio frame
    float *ibuf = nullptr;
    // buffer for output audio frame
    float *obuf = nullptr;
};

} // namespace PitchShifting
