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
    Stretcher(int dbgLevel = 1);

    int SetOptions(bool finer, bool realtime, int typewin, bool smoothing, bool formant,
        bool together, bool hipitch, int typethreading, int typetransient, int typedetector);
    // so far im not using time map...
    bool LoadTimeMap(std::string mapFile);
    void SetKeyFrameMap() { if (pts && timeMap.size() > 0) pts->setKeyFrameMap(timeMap); };
    // redo or before set options to correct given parameters for rubberband
    bool LoadFreqMap(std::string mapFile, bool pitchToFreq);
    // mainly works on R2(faster) engine, redo or before set options to correct given parameters for rubberband
    void SetCrispness(int crispness = -1);
    // clipping check during process frame
    void SetIgnoreClipping(bool ignore) { ignoreClipping = ignore; };

    // load input before create stretcher for time ratio and frames duration given to rubberband
    bool LoadInputFile(std::string fileName,
        int *pSampleRate, int *pChannels, int *pFormat, int64_t *pFramesCount,
        double ratio = 1.0, double duration = 0.0);

    // basically given ctor params to fullfill RubberBandStretcher
    void Create(size_t sampleRate, size_t channels, int options, double timeRatio, double pitchScale);
    // works on input file, ignore in realtime mode
    void ExpectedInputDuration(size_t samples);
    void MaxProcessSize(size_t samples);
    double FormantScale(double scale = 0.0);

    // compare to stretcher::channels and block size for buffer allocation/reallocation
    void PrepareBuffer(int inputChannels, int blockSize);
    // study loaded input file for stretcher (to pitch analyzing?), ignore in realtime mode 
    void StudyInputSound(/*int blockSize*/);
    // in realtime mode, process padding at begin to avoid fade in and get drop delay for output buffer
    void ProcessStartPad(int *pToDrop);
    // adjust rubberband pitch scale per process block, countIn will align to freqMap key and increase freqMap iterator
    void ApplyFreqMap(size_t countIn, int blockSize, int *pAdjustedBlockSize = nullptr);
    // process given block of sound file, NOTE: high relavent to sndfile seeking position
    // TODO: we may need change return parameter to class scoped variable instead of caller?
    // pFrame represent current process input frame position to ensure is final for rubberband process() call
    bool ProcessInputSound(int blockSize, int *pToDrop, int *pFrames,
        size_t *pCountIn, size_t *pCountOut, float gain = 1.f);
protected:
    virtual ~Stretcher();
private:
    RubberBandStretcher *pts;
    RubberBandStretcher::Options options = 0;
    
    double ratio = 1.0;
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

    enum {
        NoTransients,
        BandLimitedTransients,
        Transients
    } transients = Transients;

    enum {
        CompoundDetector,
        PercussiveDetector,
        SoftDetector
    } detector = CompoundDetector;

    bool ignoreClipping = false;

    // sound file for input wav
    SNDFILE *sndfile = nullptr;
    // input sound info
    SF_INFO sfinfo;

    int inputChannels = 0;
    int blockSize = 1024;
    // buffer reallocation depends on channels and block size
    bool reallocateBuffer = false;
    // buffer for rubberband calculation
    float **cbuf = nullptr;
    // buffer for input audio frame
    float *ibuf = nullptr;
};

Stretcher::Stretcher(int debugLevel) {
    debug = debugLevel;
    quiet = false; // TODO: or depends on debug level?
    RubberBand::RubberBandStretcher::setDefaultDebugLevel(debugLevel);
    pts = nullptr;
}

Stretcher::~Stretcher() {
    
}

int
Stretcher::SetOptions(bool finer, bool realtime, int typewin, bool smoothing, bool formant,
    bool together, bool hqpitch, int typethreading, int typetransient, int typedetector) {
    
    Stretcher::finer = finer;
    Stretcher::faster = !finer;
    if (finer) {
        options = RubberBandStretcher::OptionEngineFiner;
    }

    Stretcher::realtime = realtime;
    Stretcher::lamination = lamination;
    Stretcher::longwin = (typewin == 2);
    Stretcher::shortwin = (typewin == 1);
    Stretcher::smoothing = smoothing;
    Stretcher::formant = formant;
    Stretcher::together = together;
    if (realtime)    options |= RubberBandStretcher::OptionProcessRealTime;
    if (!lamination) options |= RubberBandStretcher::OptionPhaseIndependent;
    if (longwin)     options |= RubberBandStretcher::OptionWindowLong;
    if (shortwin)    options |= RubberBandStretcher::OptionWindowShort;
    if (smoothing)   options |= RubberBandStretcher::OptionSmoothingOn;
    if (formant)     options |= RubberBandStretcher::OptionFormantPreserved;
    if (together)    options |= RubberBandStretcher::OptionChannelsTogether;

    if (freqMap.size() > 0) {
        options |= RubberBandStretcher::OptionPitchHighConsistency;
        if (hqpitch) {
            cerr << "WARNING: High-quality pitch mode selected, but frequency or pitch map file is" << endl;
            cerr << "         provided -- pitch mode will be overridden by high-consistency mode" << endl;
            hqpitch = false;
        }
    } else if (hqpitch) {
        options |= RubberBandStretcher::OptionPitchHighQuality;
    }
    Stretcher::hqpitch = hqpitch;

    Stretcher::threading = typethreading;
    switch (threading) {
    case 0:
        options |= RubberBandStretcher::OptionThreadingAuto;
        break;
    case 1:
        options |= RubberBandStretcher::OptionThreadingNever;
        break;
    case 2:
        options |= RubberBandStretcher::OptionThreadingAlways;
        break;
    }

    switch (typetransient) {
    case 0:
        Stretcher::transients = NoTransients;
        options |= RubberBandStretcher::OptionTransientsSmooth;
        break;
    case 1:
        Stretcher::transients = BandLimitedTransients;
        options |= RubberBandStretcher::OptionTransientsMixed;
        break;
    case 2:
        Stretcher::transients = Transients;
        options |= RubberBandStretcher::OptionTransientsCrisp;
        break;
    }

    switch (typedetector) {
    case 0:
        Stretcher::detector = CompoundDetector;
        options |= RubberBandStretcher::OptionDetectorCompound;
        break;
    case 1:
        Stretcher::detector = PercussiveDetector;
        options |= RubberBandStretcher::OptionDetectorPercussive;
        break;
    case 2:
        Stretcher::detector = SoftDetector;
        options |= RubberBandStretcher::OptionDetectorSoft;
        break;
    }
    
    return options;
}

bool
Stretcher::LoadTimeMap(std::string timeMapFile) {
    // code block duplicated...
    if (timeMapFile.empty()) {
        return false;
    }
    std::ifstream ifile(timeMapFile.c_str());
    if (!ifile.is_open()) {
        cerr << "ERROR: Failed to open time map file \""
                << timeMapFile << "\"" << endl;
        return false;
    }
    std::string line;
    int lineno = 0;
    while (!ifile.eof()) {
        std::getline(ifile, line);
        while (line.length() > 0 && line[0] == ' ') {
            line = line.substr(1);
        }
        if (line == "") {
            ++lineno;
            continue;
        }
        std::string::size_type i = line.find_first_of(" ");
        if (i == std::string::npos) {
            cerr << "ERROR: Time map file \"" << timeMapFile
                    << "\" is malformed at line " << lineno << endl;
            return false;
        }
        size_t source = atoi(line.substr(0, i).c_str());
        while (i < line.length() && line[i] == ' ') ++i;
        size_t target = atoi(line.substr(i).c_str());
        timeMap[source] = target;
        if (debug > 0) {
            cerr << "adding mapping from " << source << " to " << target << endl;
        }
        ++lineno;
    }
    ifile.close();

    if (!quiet) {
        cerr << "Read " << timeMap.size() << " line(s) from time map file" << endl;
    }
    return true;
}

bool
Stretcher::LoadFreqMap(std::string mapFile, bool pitchToFreq) {
    if (mapFile.empty()) {
        return false;
    }
    std::ifstream ifile(mapFile.c_str());
    if (!ifile.is_open()) {
        cerr << "ERROR: Failed to open map file \"" << mapFile << "\"" << endl;
        return 1;
    }
    std::string line;
    int lineno = 0;
    while (!ifile.eof()) {
        std::getline(ifile, line);
        while (line.length() > 0 && line[0] == ' ') {
            line = line.substr(1);
        }
        if (line == "") {
            ++lineno;
            continue;
        }
        std::string::size_type i = line.find_first_of(" ");
        if (i == std::string::npos) {
            cerr << "ERROR: Map file \"" << mapFile
                    << "\" is malformed at line " << lineno << endl;
            return false;
        }
        size_t source = atoi(line.substr(0, i).c_str());
        while (i < line.length() && line[i] == ' ') ++i;
        double freq = atof(line.substr(i).c_str());
        if (pitchToFreq) {
            freq = pow(2.0, freq / 12.0);
        }
        freqMap[source] = freq;
        if (debug > 0) {
            cerr << "adding mapping for source frame " << source << " of frequency multiplier " << freq << endl;
        }
        ++lineno;
    }
    ifile.close();

    if (!quiet) {
        cerr << "Read " << freqMap.size() << " line(s) from frequency map file" << endl;
    }
    // reset iterator
    freqMapItr = freqMap.begin();
    return true;
}

void
Stretcher::SetCrispness(int crispness) {
    cerr << "WARNING: The crispness option effects with transients, lamination or window options" << endl;
    cerr << "         provided -- crispness will override these other options" << endl;
    Stretcher::crispness = crispness;
    switch (crispness) {
    case -1: crispness = 5; break;
    case 0: detector = CompoundDetector; transients = NoTransients; lamination = false; longwin = true; shortwin = false; break;
    case 1: detector = SoftDetector; transients = Transients; lamination = false; longwin = true; shortwin = false; break;
    case 2: detector = CompoundDetector; transients = NoTransients; lamination = false; longwin = false; shortwin = false; break;
    case 3: detector = CompoundDetector; transients = NoTransients; lamination = true; longwin = false; shortwin = false; break;
    case 4: detector = CompoundDetector; transients = BandLimitedTransients; lamination = true; longwin = false; shortwin = false; break;
    case 5: detector = CompoundDetector; transients = Transients; lamination = true; longwin = false; shortwin = false; break;
    case 6: detector = CompoundDetector; transients = Transients; lamination = false; longwin = false; shortwin = true; break;
    };
}

bool
Stretcher::LoadInputFile(std::string fileName,
    int *pSampleRate, int *pChannels, int *pFormat, int64_t *pFramesCount,
    double ratio, double duration) {
    // default value for return parameters
    *pSampleRate = 0;
    *pChannels = 0;
    *pFormat = 0;
    *pFramesCount = 0;

    if (sndfile) {
        sf_close(sndfile);
        sndfile = nullptr;
    }
    memset(&sfinfo, 0, sizeof(SF_INFO));
    
    sndfile = sf_open(fileName.c_str(), SFM_READ, &sfinfo);
    if (!sndfile) {
        cerr << "ERROR: Failed to open input file \"" << fileName.c_str() << "\": "
             << sf_strerror(sndfile) << endl;
        return false;
    }

    if (sfinfo.samplerate == 0) {
        cerr << "ERROR: File lacks sample rate in header" << endl;
        return false;
    }

    if (duration != 0.0) {
        if (sfinfo.frames == 0) {
            cerr << "ERROR: File lacks frame count in header, cannot use --duration" << endl;
            return false;
        }
        double induration = double(sfinfo.frames) / double(sfinfo.samplerate);
        if (induration != 0.0) ratio = duration / induration;
    }
    Stretcher::ratio = ratio;
    Stretcher::duration = duration;
    if (Stretcher::inputChannels != sfinfo.channels) {
        Stretcher::reallocateBuffer = true;
        Stretcher::inputChannels = sfinfo.channels;
    }
    *pSampleRate = sfinfo.samplerate;
    *pChannels = sfinfo.channels;
    *pFormat = sfinfo.format;
    *pFramesCount = sfinfo.frames;

    return true;
}

void 
Stretcher::Create(size_t sampleRate, size_t channels, int options, double timeRatio, double pitchScale) {
    if (pts) {
        delete pts;
    }
    pts = new RubberBand::RubberBandStretcher(sampleRate, channels, options, timeRatio, pitchScale);
    Stretcher::ratio = timeRatio;
    Stretcher::frequencyshift = pitchScale;
}

void
Stretcher::ExpectedInputDuration(size_t samples) {
    if (pts) {
        pts->setExpectedInputDuration(samples);
    }
}

void
Stretcher::MaxProcessSize(size_t samples) {
    if (pts) {
        pts->setMaxProcessSize(samples);
    }
}

double
Stretcher::FormantScale(double scale) {
    if (!pts) return 0;

    // TODO: can go over with default freq shifting if formant option set for preserved
    // if (scale == 0.0)
    //     scale = 1.0 / frequencyshift;
    if (scale > 0) {
        pts->setFormantScale(scale);
    }
    return pts->getFormantScale();
}

void
Stretcher::PrepareBuffer(int inputChannels, int blockSize) {
    if (reallocateBuffer || 
        Stretcher::inputChannels != inputChannels ||
        Stretcher::blockSize != blockSize) {
        Stretcher::inputChannels = inputChannels;
        Stretcher::blockSize = blockSize;
        if (ibuf) {
            delete ibuf;
            ibuf = nullptr;
        }
        if (cbuf) {
            delete cbuf; // TODO: this is 2d array!
            cbuf = nullptr;
        }
    }

    if (!ibuf) {
        ibuf = new float[inputChannels * blockSize];
    }
    if (!cbuf) {
        cbuf = new float *[inputChannels];
        for (size_t c = 0; c < inputChannels; ++c) {
            cbuf[c] = new float[blockSize];
        }
    }
}

void
Stretcher::StudyInputSound(/*int blockSize*/) {
    if (!pts) return;

    // check mode and loaded input sound
    if (realtime) {
        cerr << "Pass 1: Studying... is unavailable in realtime mode" << endl;
        return;
    }
    if (!sndfile || !sfinfo.frames || !sfinfo.samplerate ) {
        cerr << "ERROR: Load input sound file before calls to study it" << endl;
        return;
    }
    // reset file position
    sf_seek(sndfile, 0, SEEK_SET);
    int channels = sfinfo.channels;
    PrepareBuffer(channels, Stretcher::blockSize);

    int frame = 0;
    int percent = 0;

    if (!quiet) {
        cerr << "Pass 1: Studying..." << endl;
    }

    bool final = false;
    
    while (!final) {

        int count = -1;
        if ((count = sf_readf_float(sndfile, ibuf, blockSize)) < 0) break;

        for (size_t c = 0; c < channels; ++c) {
            for (int i = 0; i < count; ++i) {
                cbuf[c][i] = ibuf[i * channels + c];
            }
        }

        final = (frame + blockSize >= sfinfo.frames);
        if (count == 0) {
            final = true;
        }

        pts->study(cbuf, count, final);

        int p = int((double(frame) * 100.0) / sfinfo.frames);
        if (p > percent || frame == 0) {
            percent = p;
            if (!quiet) {
                cerr << "\r" << percent << "% ";
            }
        }

        frame += blockSize;
    }

    if (!quiet) {
        cerr << "\rCalculating profile..." << endl;
    }

    sf_seek(sndfile, 0, SEEK_SET);
}

void
Stretcher::ProcessStartPad(int *pToDrop) {
    *pToDrop = 0;
    if (!pts) return;
    *pToDrop = pts->getStartDelay();
    if (!realtime) return;

    PrepareBuffer(inputChannels, blockSize);

    int toPad = pts->getPreferredStartPad();
    if (debug > 0) {
        cerr << "padding start with " << toPad
            << " samples in RT mode, will drop " << *pToDrop
            << " at output" << endl;
    }
    if (toPad > 0) {
        for (size_t c = 0; c < inputChannels; ++c) {
            for (int i = 0; i < blockSize; ++i) {
                cbuf[c][i] = 0.f;
            }
        }
        while (toPad > 0) {
            int p = toPad;
            if (p > blockSize) p = blockSize;
            pts->process(cbuf, p, false);
            toPad -= p;
        }
    }
}

void
Stretcher::ApplyFreqMap(size_t countIn, int blockSize, int *pAdjustedBlockSize) {
    // useless if no freqMap
    if (freqMap.size() == 0) return;
    if (!pts) return;

    while (freqMapItr != freqMap.end()) {
        size_t nextFreqFrame = freqMapItr->first;
        // iterate key frame to counted input frame and apply the target pitch
        if (nextFreqFrame <= countIn) {
            double s = frequencyshift * freqMapItr->second;
            if (debug > 0) {
                cerr << "at frame " << countIn
                    << " (requested at " << freqMapItr->first
                    << " [NOT] plus latency " << pts->getLatency()
                    << ") updating frequency ratio to " << s << endl;
            }
            pts->setPitchScale(s);
            ++freqMapItr;
        } else {
            // based on next key frame, effect to the next block size of next reading frame,
            // or simply ignore with consistent block size 
            if (nextFreqFrame < countIn + blockSize) {
                if (pAdjustedBlockSize)
                    *pAdjustedBlockSize = nextFreqFrame - countIn;
            }
            break;
        }
    }
}

bool
Stretcher::ProcessInputSound(int blockSize, int *pToDrop, int *pFrame,
    size_t *pCountIn, size_t *pCountOut, float gain) {
    // simply check for function refactoring
    if (!pts) return false;
    PrepareBuffer(inputChannels, blockSize);

    int count = -1;
    if ((count = sf_readf_float(sndfile, ibuf, blockSize)) < 0) {
        return false;
    }

    *pCountIn += count;

    for (size_t c = 0; c < inputChannels; ++c) {
        for (int i = 0; i < count; ++i) {
            cbuf[c][i] = ibuf[i * inputChannels + c];
        }
    }

    // TODO: without sndfile, we may not be able to ensure the final process
    bool isFinal = (*pFrame + blockSize >= sfinfo.frames);

    if (count == 0) {
        if (debug > 1) {
            cerr << "at frame " << *pFrame << " of " << sfinfo.frames << ", read count = " << count << ": marking final as true" << endl;
        }
        isFinal = true;
    }
    
    if (debug > 2) {
        cerr << "count = " << count << ", bs = " << blockSize << ", frame = " << *pFrame << ", frames = " << sfinfo.frames << ", final = " << isFinal << endl;
    }

    pts->process(cbuf, count, isFinal);

    int avail;
    bool clipping = false;
    while ((avail = pts->available()) > 0) {
        if (debug > 1) {
            cerr << "available = " << avail << endl;
        }
        // reuse blockSize variable to check output frame, and limited in constraint 1024
        blockSize = avail;
        if (blockSize > Stretcher::blockSize) {
            blockSize = Stretcher::blockSize;
        }
        
        if (*pToDrop > 0) {
            int dropHere = *pToDrop;
            // dropping frame can only less than adjusted block size(same with avail but less than 1024)
            if (dropHere > blockSize) {
                dropHere = blockSize;
            }
            if (debug > 1) {
                cerr << "toDrop = " << *pToDrop << ", dropping "
                        << dropHere << " of " << avail << endl;
            }
            pts->retrieve(cbuf, dropHere);
            *pToDrop -= dropHere;
            avail -= dropHere;
            // to next available processed block
            continue;
        }
        
        if (debug > 2) {
            cerr << "retrieving block of " << blockSize << endl;
        }
        pts->retrieve(cbuf, blockSize);
        
        // NOTE: this realtime setting is used for input sound file(which has final flag from reading frame)
        if (realtime && isFinal) {
            // (in offline mode the stretcher handles this itself)
            size_t ideal = size_t(*pCountIn * ratio);
            if (debug > 2) {
                cerr << "at end, ideal = " << ideal
                    << ", countOut = " << *pCountOut
                    << ", thisBlockSize = " << blockSize << endl;
            }
            if (*pCountOut + blockSize > ideal) {
                blockSize = ideal - *pCountOut;
                if (debug > 1) {
                    cerr << "truncated final block to " << blockSize
                        << endl;
                }
            }
        }
        
        *pCountOut += blockSize;
        // reuse ibuf to write output frame
        // TODO: we should use different variable for output channel settings and buffer
        int channels = Stretcher::inputChannels;
        for (size_t c = 0; c < channels; ++c) {
            for (int i = 0; i < blockSize; ++i) {
                float value = gain * cbuf[c][i];
                if (ignoreClipping) { // i.e. just clamp, don't bail out
                    if (value > 1.f) value = 1.f;
                    if (value < -1.f) value = -1.f;
                } else {
                    if (value >= 1.f || value < -1.f) {
                        clipping = true;
                        gain = (0.999f / fabsf(cbuf[c][i]));
                    }
                }
                ibuf[i * channels + c] = value;
            }
        }
        // TODO: need output file initialization
        //sf_writef_float(sndfileOut, ibuf, blockSize);
    }

    if (clipping) {
        const float mingain = 0.75f;
        if (gain < mingain) {
            cerr << "NOTE: Clipping detected at output sample "
                    << *pCountOut << ", but not reducing gain as it would "
                    << "mean dropping below minimum " << mingain << endl;
            gain = mingain;
            ignoreClipping = true;
        } else {
            if (!quiet) {
                cerr << "NOTE: Clipping detected at output sample "
                        << *pCountOut << ", restarting with "
                        << "reduced gain of " << gain
                        << " (supply --ignore-clipping to avoid this)"
                        << endl;
            }
        }
        //TODO: if clipping detected, need redo process from beginning with new gain
        // successful = false;
        // break;
        return false;
    }

    return true;
}


} //namespace PitchShifting