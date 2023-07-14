#include "stretcher.hpp"
#ifdef _MSC_VER
// for usleep from unistd
#include <windows.h>
static void usleep(unsigned long usec) {
    ::Sleep(usec == 0 ? 0 : usec < 1000 ? 1 : usec / 1000);
}
// TODO: for std::max/min in cross-platform design
// undefine minwindef.h min/max marco and use stl is considered
#else
#include <unistd.h>
#endif

namespace PitchShifting {

Stretcher::Stretcher(int defBlockSize, int debugLevel) {
    // for port audio initialization
    Pa_Initialize();
    inStream = nullptr;
    outStream = nullptr;
    //chunkWrite = 0;

    debug = debugLevel;
    quiet = (debugLevel < 2);
    cerr << "Constructor default block size:" << defBlockSize << " debug:" << debugLevel << " quiet:" << quiet << endl;

    RubberBand::RubberBandStretcher::setDefaultDebugLevel(debugLevel);
    
    Stretcher::pts = nullptr;
    Stretcher::options = 0;
    // use my own default
    ratio = 1.0;
    duration = 0.0;
    frequencyshift = 1.0;
    realtime = true;
    threading = 0;
    lamination = true;
    longwin = false;
    shortwin = false;
    smoothing = true;
    hqpitch = false;
    formant = true;
    together = false;
    crispness = -1;
    faster = false;
    finer = true;

    freqMapItr = freqMap.begin();
    transients = Transients;
    detector = CompoundDetector;

    sndfileIn = nullptr;
    sndfileOut = nullptr;

    inputChannels = 0;
    outputChannels = 0;
    Stretcher::defBlockSize = defBlockSize;
    
    cbuf = nullptr;
    ibuf = nullptr;
    obuf = nullptr;

    debugInMaxVal = 0.f;
    inGain = 1.f;

    // we need channels, blocksize to initialize ringbuffer(2dim)
    inBuffer = nullptr;
    outBuffer = nullptr;

    dropFrames = 0;
    ignoreClipping = true;
    outGain = 1.f;
}

Stretcher::~Stretcher() {
    cerr << "Stretcher dector..." << endl;
    // TODO: cleanup something 

    dispose();
}

void
Stretcher::dispose() {
    if (ibuf) {
        delete ibuf;
        ibuf = nullptr;
    }
    if (cbuf) {
        for (int c = 0; c < inputChannels; c++) {
            delete[] cbuf[c];
        }
        delete[] cbuf;
        cbuf = nullptr;
    }
    if (obuf) {
        delete obuf;
        obuf = nullptr;
    }
    if (inBuffer) {
        delete inBuffer;
        inBuffer = nullptr;
    }
    if (outBuffer) {
        delete outBuffer;
        outBuffer = nullptr;
    }
    Pa_Terminate();
}

int
Stretcher::SetOptions(bool finer, bool realtime, int typewin, bool smoothing, bool formant,
    bool together, bool hqpitch, bool lamination,
    int typethreading, int typetransient, int typedetector) {
    options = 0;

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
Stretcher::SetCrispness(int crispness, int *ptypewin, bool *plamination, int *ptypetransient, int *ptypedetector) {
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
    if (ptypewin) {
        *ptypewin = (shortwin) ? 1 : (longwin) ? 2 : 0/*standard*/;
    }
    if (plamination) {
        *plamination = lamination;
    }
    if (ptypetransient) {
        *ptypetransient = transients;
    }
    if (ptypedetector) {
        *ptypedetector = detector;
    }
}

int
Stretcher::GetFileFormat(std::string extName) {
    // duplicated from original code blocks 
    std::string ex = extName;
    for (size_t i = 0; i < ex.size(); ++i) {
        ex[i] = tolower(ex[i]);
    }
    int types = 0;
    (void)sf_command(0, SFC_GET_FORMAT_MAJOR_COUNT, &types, sizeof(int));
    bool found = false;
    int format = 0;
    for (int i = 0; i < types; ++i) {
        SF_FORMAT_INFO info;
        info.format = i;
        if (sf_command(0, SFC_GET_FORMAT_MAJOR, &info, sizeof(info))) {
            continue;
        } else {
            if (ex == std::string(info.extension)) {
                format = info.format | SF_FORMAT_PCM_24;
                found = true;
                break;
            }
        }
    }
    if (!found) {
        cerr << "NOTE: Unknown file extension \"" << extName
                << "\", will use same file format as input file" << endl;
    }
    return format;
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

    if (sndfileIn) {
        sf_close(sndfileIn);
        sndfileIn = nullptr;
    }
    memset(&sfinfoIn, 0, sizeof(SF_INFO));
    
    sndfileIn = sf_open(fileName.c_str(), SFM_READ, &sfinfoIn);
    if (!sndfileIn) {
        cerr << "ERROR: Failed to open input file \"" << fileName.c_str() << "\": "
             << sf_strerror(sndfileIn) << endl;
        return false;
    }

    if (sfinfoIn.samplerate == 0) {
        cerr << "ERROR: File lacks sample rate in header" << endl;
        return false;
    }

    if (duration != 0.0) {
        if (sfinfoIn.frames == 0) {
            cerr << "ERROR: File lacks frame count in header, cannot use --duration" << endl;
            return false;
        }
        double induration = double(sfinfoIn.frames) / double(sfinfoIn.samplerate);
        if (induration != 0.0) ratio = duration / induration;
    }
    Stretcher::ratio = ratio;
    Stretcher::duration = duration;
    if (Stretcher::inputChannels != sfinfoIn.channels) {
        PrepareInputBuffer(sfinfoIn.channels, defBlockSize, reserveBuffer);
        Stretcher::inputChannels = sfinfoIn.channels;
    }
    *pSampleRate = sfinfoIn.samplerate;
    *pChannels = sfinfoIn.channels;
    *pFormat = sfinfoIn.format;
    *pFramesCount = sfinfoIn.frames;

    return true;
}

bool
Stretcher::SetOutputFile(std::string fileName,
    int sampleRate, int channels, int format) {

    if (sndfileOut) {
        sf_close(sndfileOut);
        sndfileOut = nullptr;
    }
    memset(&sfinfoOut, 0, sizeof(SF_INFO));
    
    // TODO: the input sample rate also affact rubberband stretcher,
    //       force align from input for ease of calculation to output frames and duration.
    if (/*sampleRate == 0 && */sfinfoIn.samplerate == 0) {
        cerr << "ERROR: Missing sample rate assignment or none of reference from input" << endl;
        return false;
    }
    if (sampleRate != sfinfoIn.samplerate) {
        cerr << "WARNING: Force align sample rate from input " << sfinfoIn.samplerate << " because required ideal solution so far" << endl;
        sampleRate = sfinfoIn.samplerate;
    }

    if (channels == 0 && sfinfoIn.channels == 0) {
        cerr << "ERROR: Missing channels assignment or none of reference from input" << endl;
        return false;
    }

    // NOTE: sndfile.h defined format, also https://svn.ict.usc.edu/svn_vh_public/trunk/lib/vhcl/libsndfile/doc/api.html 
    if (format == 0 && sfinfoIn.format == 0) {
        cerr << "ERROR: Missing format assignment or none of reference from input" << endl;
        return false;
    }

    // NOTE: the frames will be duration * sample rate
    // TODO: so far I need quickly hands on design, align from input as well
    if (sfinfoIn.frames == 0) {
        cerr << "ERROR: Missing information from input sound as reference to the output" << endl;
        return false;
    }

    sfinfoOut.channels = (channels) ? channels : sfinfoIn.channels;
    sfinfoOut.samplerate = (sampleRate) ? sampleRate : sfinfoIn.samplerate;
    sfinfoOut.frames = int(sfinfoIn.frames * ratio + 0.1);
    sfinfoOut.format = (format) ? format : sfinfoIn.format;
    sfinfoOut.sections = sfinfoIn.sections;
    sfinfoOut.seekable = sfinfoIn.seekable;

    sndfileOut = sf_open(fileName.c_str(), SFM_RDWR, &sfinfoOut);
    if (!sndfileOut) {
        cerr << "ERROR: Failed to open output file \"" << fileName.c_str() << "\": "
             << sf_strerror(sndfileOut) << endl;
        return false;
    }

    if (Stretcher::outputChannels != sfinfoOut.channels) {
        PrepareOutputBuffer(sfinfoOut.channels, defBlockSize, reserveBuffer);
        Stretcher::outputChannels = sfinfoOut.channels;
    }
    return true;
}

void 
Stretcher::Create(size_t sampleRate, int channels, int options, double timeRatio, double pitchScale) {
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
        // TODO: should align to defBlockSize
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
Stretcher::PrepareInputBuffer(int channels, int blocks, size_t reserves) {
    // cleanup exists allocation
    if (ibuf) {
        delete ibuf;
        ibuf = nullptr;
    }
    if (cbuf) {
        for (int c = 0; c < Stretcher::inputChannels; c++) {
            delete cbuf[c];
        }
        delete[] cbuf;
        cbuf = nullptr;
    }

    ibuf = new float[channels * blocks];
    cbuf = new float *[channels];
    for (int c = 0; c < channels; ++c) {
        cbuf[c] = new float[blocks];
    }

    if (inBuffer) {
        delete inBuffer;
        inBuffer = nullptr;
    }
    inBuffer = new RingBuffer<float>(channels * blocks + reserves);
}

void
Stretcher::PrepareOutputBuffer(int channels, int blocks, size_t reserves) {
    // cleanup exists allocation
    if (obuf) {
        delete obuf;
        obuf = nullptr;
    }
    obuf = new float[channels * blocks];

    if (outBuffer) {
        delete outBuffer;
        outBuffer = nullptr;
    }
    outBuffer = new RingBuffer<float>(channels * blocks + reserves);

}

void
Stretcher::StudyInputSound(/*int blockSize*/) {
    if (!pts) return;

    // check mode and loaded input sound
    if (realtime) {
        cerr << "Pass 1: Studying... is unavailable in realtime mode" << endl;
        return;
    }
    if (!sndfileIn || !sfinfoIn.frames || !sfinfoIn.samplerate ) {
        cerr << "ERROR: Load input sound file before calls to study it" << endl;
        return;
    }
    // reset file position
    sf_seek(sndfileIn, 0, SEEK_SET);
    int channels = sfinfoIn.channels;

    int frame = 0;
    int percent = 0;

    if (!quiet) {
        cerr << "Pass 1: Studying..." << endl;
    }

    bool final = false;
    int blockSize = Stretcher::defBlockSize; // only for original code design
    
    while (!final) {

        int count = -1;
        if ((count = sf_readf_float(sndfileIn, ibuf, blockSize)) < 0) break;

        for (size_t c = 0; c < channels; ++c) {
            for (int i = 0; i < count; ++i) {
                cbuf[c][i] = ibuf[i * channels + c];
            }
        }

        final = (frame + blockSize >= sfinfoIn.frames);
        if (count == 0) {
            final = true;
        }

        pts->study(cbuf, count, final);

        int p = int((double(frame) * 100.0) / sfinfoIn.frames);
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

    sf_seek(sndfileIn, 0, SEEK_SET);
}

int
Stretcher::ProcessStartPad() {
    if (!pts) return 0;
    int toDrop = pts->getStartDelay();
    if (!realtime) return toDrop;

    int blockSize = Stretcher::defBlockSize; // only for original code design

    int toPad = pts->getPreferredStartPad();
    if (debug > 0) {
        cerr << "padding start with " << toPad
            << " samples in RT mode, will drop " << toDrop
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

    return toDrop;
}

void
Stretcher::ApplyFreqMap(size_t countIn,/* int blockSize,*/ int *pAdjustedBlockSize) {
    // useless if no freqMap
    if (freqMap.size() == 0) return;
    if (!pts) return;

    int blockSize = Stretcher::defBlockSize; // only for original code design
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
Stretcher::ProcessInputSound(/*int blockSize, */int *pFrame, size_t *pCountIn) {
    // simply check for function refactoring
    if (!pts) return false;

    // for original code design, also be reused for retrieve data behavior
    int blockSize = Stretcher::defBlockSize;

    int count = -1;
    // separate by file or by stream
    if (sndfileIn) {
        if ((count = sf_readf_float(sndfileIn, ibuf, blockSize)) < 0) {
            return false;
        }
        if (sfinfoIn.channels * count > inBuffer->getWriteSpace()) {
            cerr << "input buffer is full" << endl;
        } else {
            inBuffer->write(ibuf, sfinfoIn.channels * count);
        }
    }
    if (inStream) {
        std::lock_guard<std::mutex> lock(inMutex);
        // TODO: temerary ignore in my macbook prevent debugger console freeze
#ifdef _WIN32
        // cerr << "!!! Read in chunks " << inChunks.size() << endl;
#endif
        // if (inChunks.size() > 0) {
        //     auto chunk = inChunks.front();
        //     memcpy(ibuf, chunk, sizeof(float) * blockSize * inputChannels);
        //     inChunks.pop_front();
            count = blockSize;
        //     delete chunk;
        //     // TODO: find a way to drop frame if can not handle
        // }
        // else {
        //     return false; // TODO: need return define to caller to stop while-loop for reading frames
        // }

        if (inputChannels * count > inBuffer->getReadSpace()) {
            return false; // buffer not enough for process
        }
        else {
            inBuffer->read(ibuf, inputChannels * count);
            // debug
            if (debugBuffer && debugBufTimerIn != (*pCountIn / 48000)) {
                debugBufTimerIn = (*pCountIn / 48000);/*a samplerate*/
                cerr << "input buffer usage " << (int)(((float)inBuffer->getReadSpace() / inBuffer->getSize()) * 100.f) << "%" << endl;
            }
        }
    }

    bool debugMax = false;
    float value;
    for (int c = 0; c < inputChannels; ++c) {
        for (int i = 0; i < count; ++i) {
            value = inGain * ibuf[i * inputChannels + c];
            if (value > 1.f) value = 1.f;
            if (value < -1.f) value = -1.f;
            cbuf[c][i] = value;
            if (debugBuffer && debugInMaxVal < ibuf[i * inputChannels + c]) {
                debugInMaxVal = ibuf[i * inputChannels + c];
                debugMax = true;
            }
        }
    }
    if (debugMax) {
        cerr << "=== Input max value " << debugInMaxVal << ", gained " << debugInMaxVal * inGain << endl;
    }

    // NOTE: countIn will be the same with total frames from input
    *pCountIn += count;

    // separate by file or by stream
    bool isFinal = false;
    if (sndfileIn) {
        isFinal = (*pFrame + blockSize >= sfinfoIn.frames);

        if (count == 0) {
            if (debug > 1) {
                cerr << "at frame " << *pFrame << " of " << sfinfoIn.frames << ", read count = " << count << ": marking final as true" << endl;
            }
            isFinal = true;
        }

        if (debug > 2) {
            cerr << "in = " << *pCountIn << ", count = " << count << ", bs = " << blockSize << ", frame = " << *pFrame << ", frames = " << sfinfoIn.frames << ", final = " << isFinal << endl;
        }
    }

    pts->process(cbuf, count, isFinal);
    // increase frame number to caller
    *pFrame += count;
    // DEBUG: only process input, return isFinal as result
    return isFinal;
}

bool
Stretcher::RetrieveAvailableData(size_t *pCountOut, bool isFinal) {
    // partial simillar with seconds section of ProcessInputSound 
    // but just get rest of availble blocks
    
    // for original code design, also be reused for retrieve data behavior
    int blockSize = Stretcher::defBlockSize;
    int avail;
    bool clipping = false;
    while ((avail = pts->available()) >= 0) {
        if (debug > 1) {
            if (isFinal) {
                cerr << "(completing) ";
            }
            cerr << "available = " << avail << endl;
        }

        // original while loop exit
        if (avail == 0 && !isFinal) {
            break;
        }
        if (avail == 0 && isFinal) {
            if (realtime ||
                (options & RubberBandStretcher::OptionThreadingNever)) {
                break;
            } else {
                usleep(10000);
            }
        }
        
        blockSize = avail;
        if (blockSize > Stretcher::defBlockSize) {
            blockSize = Stretcher::defBlockSize;
        }

        // process dropping frames (for realtime mode has padding at begin)
        if (dropFrames > 0) {
            int dropHere = dropFrames;
            // dropping frame can only less than adjusted block size(same with avail but less than 1024)
            if (dropHere > blockSize) {
                dropHere = blockSize;
            }
            if (debug > 1) {
                cerr << "toDrop = " << dropFrames << ", dropping "
                        << dropHere << " of " << avail << endl;
            }
            pts->retrieve(cbuf, dropHere);
            dropFrames -= dropHere;
            avail -= dropHere;
            // to next available processed block
            continue;
        }

        if (debug > 2) {
            cerr << "retrieving block of " << blockSize << ", out = " << *pCountOut << endl;
        }
        pts->retrieve(cbuf, blockSize);

        // process frames count alignment between input and output file in realtime mode,
        // NOTE: but it may not necessary for my real purpose w/o input and output files
        if (realtime && isFinal && sndfileIn) {
            // (in offline mode the stretcher handles this itself)
            size_t ideal = size_t(sfinfoIn.frames * ratio);
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

        float value;
        for (size_t c = 0; c < inputChannels; ++c) {
            for (int i = 0; i < blockSize; ++i) {
                value = outGain * cbuf[c][i];
                if (ignoreClipping) { // i.e. just clamp, don't bail out
                    if (value > 1.f) value = 1.f;
                    if (value < -1.f) value = -1.f;
                } else {
                    if (value >= 1.f || value < -1.f) {
                        clipping = true;
                        outGain = (0.999f / fabsf(cbuf[c][i]));
                    }
                }
                cbuf[c][i] = value;
                if (c < outputChannels) {
                    obuf[i * outputChannels + c] = value;
                }
            }
        }
        // rest of channels if output has more than input
        for (int c = inputChannels; c < outputChannels; ++c) {
            for (int i = 0; i < blockSize; ++i) {
                obuf[i * outputChannels + c] = cbuf[0][i];
            }
        }

        {
            std::lock_guard<std::mutex> lock(outMutex);
            // cerr << "!!! Write out chunks " << outChunks.size() << ", chunkWrite " << chunkWrite << endl;
            // int currSize = blockSize * outputChannels;
            // auto obufPos = obuf;
            // while (currSize > 0) {
            //     if (chunkWrite == 0 || outChunks.size() == 0) {
            //         auto chunk = new float[defBlockSize * outputChannels];
            //         outChunks.push_back(chunk);
            //     }
            //     int restSize = defBlockSize * outputChannels - chunkWrite;
            //     if (currSize > restSize) {
            //         memcpy(&outChunks.back()[chunkWrite], obufPos, sizeof(float) * restSize);
            //         currSize -= restSize;
            //         obufPos += restSize;
            //         chunkWrite = 0;
            //     }
            //     else {
            //         memcpy(&outChunks.back()[chunkWrite], obufPos, sizeof(float) * currSize);
            //         chunkWrite += currSize;
            //         currSize -= currSize;
            //     }
            // }

            int writable = outBuffer->getWriteSpace();
            if (outputChannels * blockSize > writable) {
                cerr << "output buffer is full" << endl;
            }
            else {
                if (debugBuffer && debugBufTimerOut != (*pCountOut / 48000)) {
                    debugBufTimerOut = (*pCountOut / 48000);/*a samplerate*/
                    cerr << "output buffer usage " << (int)((1.f - (float)writable / outBuffer->getSize()) * 100.f) << "%" << endl;
                }
                outBuffer->write(obuf, outputChannels * blockSize);
            }
        }

        // output file before later variable changes for chunks
        if (sndfileOut) {
            sf_writef_float(sndfileOut, obuf, blockSize);
        }

    } // while (avail)

    if (clipping) {
        if (outGain < minGain) {
            cerr << "NOTE: Clipping detected at output sample "
                    << *pCountOut << ", but not reducing gain as it would "
                    << "mean dropping below minimum " << minGain << endl;
            outGain = minGain;
            ignoreClipping = true;
        } else {
            if (!quiet) {
                cerr << "NOTE: Clipping detected at output sample "
                        << *pCountOut << ", restarting with "
                        << "reduced gain of " << outGain
                        << " (supply --ignore-clipping to avoid this)"
                        << endl;
            }
        }
        // if clipping detected, need redo process from begin with modified gain
        return false;
    } // if (clipping)
    
    return true;
}

void
Stretcher::CloseFiles() {
    if (sndfileIn) {
        sf_close(sndfileIn);
        sndfileIn = nullptr;
    }
    if (sndfileOut) {
        sf_close(sndfileOut);
        sndfileOut = nullptr;
    }
}


void
Stretcher::ListAudioDevices() {
    // list host apis
    int num_apis = 0;
    num_apis = Pa_GetHostApiCount();
    const char* apis[16] = { 0 };
    for (int i = 0; i < num_apis; i++) {
        auto api_info = Pa_GetHostApiInfo(i);
        apis[i] = api_info->name;
        cerr << "API " << i << " type:" << api_info->type << " name:" << api_info->name << " idef:" << api_info->defaultInputDevice << " odef:" << api_info->defaultOutputDevice << endl;
    }

    // list all
    int num_devices = 0;
    PaErrorCode err = paNoError;
    num_devices = Pa_GetDeviceCount();
    if (num_devices < 0) {
        err = (PaErrorCode)num_devices;
        cerr << "ERROR: Pa_CountDevices returned " << err << endl;
        return;
    }

    const PaDeviceInfo* dev_info;
    for (int i = 0; i < num_devices; i++) {
        dev_info = Pa_GetDeviceInfo(i);
        cerr << "DEV " << i << " " << dev_info->name << " api:" << apis[dev_info->hostApi] << " ich:" << dev_info->maxInputChannels << " och:" << dev_info->maxOutputChannels;
        cerr << " samplerate:" << dev_info->defaultSampleRate << endl;
    }
}

int
Stretcher::inputAudioCallback(
    const void* inBuffer, void* outBuffer,
    unsigned long frames,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags flags,
    void *data
    ) {
    Stretcher *pst = (Stretcher *)data;

    float *in = (float*)inBuffer;
    // TODO: may quick check levels to drop frames prevent too many input can't process immediately
    {
        std::lock_guard<std::mutex> lock(pst->inMutex);
        // cerr << "!!! Write in chunks " << pst->inChunks.size() << endl;
        // auto tmp = new float[frames * pst->inputChannels];
        // memcpy(tmp, in, sizeof(float) * frames * pst->inputChannels);
        // pst->inChunks.push_back(tmp);
        int writable = pst->inBuffer->getWriteSpace();
        if (pst->inputChannels * frames > writable) {
            cerr << "input buffer is full" << endl;
        } else {
            // move to process function
            /*if (pst->debugBuffer) {
                cerr << "input buffer usage " << (int)((1.f - (float)writable / pst->inBuffer->getSize()) * 100.f) << "%" << endl;
            }*/
            pst->inBuffer->write(in, pst->inputChannels * frames);
        }
    }

    return paContinue;
}

int
Stretcher::outputAudioCallback(
        const void* inBuffer, void* outBuffer,
        unsigned long frames,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags flags,
        void *data
    ){
    Stretcher *pst = (Stretcher *)data;

    //float *in = (float*)inBuffer;
    float *out = (float*)outBuffer;

    {
        std::lock_guard<std::mutex> lock(pst->outMutex);

        // cerr << "!!! Read out chunks " << pst->outChunks.size() << endl;
        // if (pst->outDelayFrames > 0) {
        //     if (pst->outChunks.size() > pst->outDelayFrames / pst->defBlockSize)
        //         pst->outDelayFrames = 0;
        // }
        // if (pst->outDelayFrames <= 0 && pst->outChunks.size() > 0) {
        //     auto chunk = pst->outChunks.front();
        //     memcpy(out, chunk, sizeof(float) * frames * pst->outputChannels);
        //     pst->outChunks.pop_front();
        //     delete chunk;
        // }
        if (pst->outDelayFrames > 0) {
            pst->outDelayFrames -= frames;
            if (pst->outDelayFrames < 0) {
                cerr << "=== Start reading outputs ====" << endl;
            }
            return paContinue;
        }
        if (pst->outputChannels * frames > pst->outBuffer->getReadSpace()) {
            cerr << "output buffer is not enough" << endl;
        } else {
            pst->outBuffer->read(out, pst->outputChannels * frames);
        }
    }

    return paContinue;
}

bool
Stretcher::SetInputStream(int index, int *pSampleRate, int *pChannels) {

    const PaDeviceInfo* inInfo = Pa_GetDeviceInfo(index);
    if (!inInfo) {
        cerr << "No device found at " << index << endl;
        return false;
    }
    cerr << "IN " << index << " " << inInfo->name << " api:" << Pa_GetHostApiInfo(inInfo->hostApi)->name << " ich:" << inInfo->maxInputChannels << " och:" << inInfo->maxOutputChannels;
    cerr << " samplerate:" << inInfo->defaultSampleRate << " input delay:" << inInfo->defaultLowInputLatency << endl;

    PaStreamParameters inParam;
    memset(&inParam, 0, sizeof(inParam));
    inParam.channelCount = inInfo->maxInputChannels;
    inParam.device = index;
    inParam.sampleFormat = paFloat32;
    inParam.suggestedLatency = inInfo->defaultLowInputLatency;
    inParam.hostApiSpecificStreamInfo = NULL;

    PaError er = Pa_OpenStream(
        &inStream,
        &inParam,
        nullptr,
        inInfo->defaultSampleRate,
        Stretcher::defBlockSize,
        paNoFlag,
        inputAudioCallback,
        (void *)this
    );
    cerr << "Open input stream result " << er << endl;

    // NOTE: overwrite to file input
    if (Stretcher::inputChannels != inInfo->maxInputChannels) {
        PrepareInputBuffer(inInfo->maxInputChannels, defBlockSize, reserveBuffer);
        Stretcher::inputChannels = inInfo->maxInputChannels;
    }

    if (pSampleRate) *pSampleRate = inInfo->defaultSampleRate;
    if (pChannels) *pChannels = inInfo->maxInputChannels;

    return er == paNoError;
}

bool
Stretcher::SetOutputStream(int index) {

    const PaDeviceInfo* outInfo = Pa_GetDeviceInfo(index);
    if (!outInfo) {
        cerr << "No device found at " << index << endl;
        return false;
    }
    cerr << "OUT " << index << " " << outInfo->name << " api:" << Pa_GetHostApiInfo(outInfo->hostApi)->name << " ich:" << outInfo->maxInputChannels << " och:" << outInfo->maxOutputChannels;
    cerr << " samplerate:" << outInfo->defaultSampleRate << " output delay:" << outInfo->defaultLowOutputLatency << endl;
    
    PaStreamParameters outParam;
    memset(&outParam, 0, sizeof(outParam));
    outParam.channelCount = outInfo->maxOutputChannels;
    outParam.device = index;
    outParam.sampleFormat = paFloat32;
    outParam.suggestedLatency = outInfo->defaultLowOutputLatency;
    outParam.hostApiSpecificStreamInfo = NULL;

    PaError er = Pa_OpenStream(
        &outStream,
        nullptr,
        &outParam,
        outInfo->defaultSampleRate,
        Stretcher::defBlockSize,
        paNoFlag,
        outputAudioCallback,
        (void *)this
    );
    cerr << "Open output stream result " << er << endl;

    // NOTE: overwtie to file output
    if (Stretcher::outputChannels != outInfo->maxOutputChannels) {
        PrepareOutputBuffer(outInfo->maxOutputChannels, defBlockSize, reserveBuffer);
        Stretcher::outputChannels = outInfo->maxOutputChannels;
    }

    return er == paNoError;
}


} //namespace PitchShifting