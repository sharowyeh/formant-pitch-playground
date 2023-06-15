#include "rubberband/RubberBandStretcher.h"
#include <iostream>

#include <cmath>
#include <time.h>
#include <cstdlib>
#include <cstring>
#include <string>

namespace PitchShifting {

class Stretcher {
public:
    Stretcher(int dbgLevel = 1);
    // basically given ctor params to fullfill RubberBandStretcher
    void Initialize(size_t sampleRate, size_t channels, int options, double timeRatio, double pitchScale);
protected:
    virtual ~Stretcher();
private:
    RubberBand::RubberBandStretcher *pts;

    double ratio = 1.0;
    double duration = 0.0;
    double pitchshift = 0.0;
    double frequencyshift = 1.0;
    int debug = 0;
    bool realtime = false;
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
};

Stretcher::Stretcher(int debugLevel) {
    debug = debugLevel;
    RubberBand::RubberBandStretcher::setDefaultDebugLevel(debugLevel);
    pts = nullptr;
}

Stretcher::~Stretcher() {
    
}

void 
Stretcher::Initialize(size_t sampleRate, size_t channels, int options, double timeRatio, double pitchScale) {
    if (pts) {
        delete pts;
    }
    pts = new RubberBand::RubberBandStretcher(sampleRate, channels, options, timeRatio, pitchScale);
}



} //namespace PitchShifting