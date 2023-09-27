/* pitch shifting application parameters, for both console and GUI modes */
#pragma once
#include <string>

namespace PitchShifting {

class Parameters {
public:
    bool gui = false;
    // estimate frame ratio from input sound for output, default 1.0
    double timeratio = 1.0;
    // estimate output sound file duration, default unset(0.0)
    double duration = 0.0;
    double pitchshift = 0.0;
    // percentage, default 1.0
    double frequencyshift = 1.0;
    double formantshift = 0.0; //semitones
    int debug = 3;
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
    bool listdev = false;
    double inputgaindb = 0.0; // dB

    bool haveRatio = false;

    std::string timeMapFile;
    std::string freqMapFile;
    std::string pitchMapFile;
    bool freqOrPitchMapSpecified = false;

    int transients = 2;/*Transients*/
    int detector = 0;/*CompoundDetector*/

    bool ignoreClipping = false;

    std::string myName;
    bool isR3;

    char* inAudioParam;
    char* outAudioParam;

    int typewin = 0/*0=OptionWindowStandard*/;

    Parameters(int c = 0, char** v = nullptr);
    /* 
     * given CLI arguments will overwrite arg from constructure
     * \return 1=failure, -1=pass(continue), 0=pass(leave process)
     */
    int ParseOptions(int c = 0, char** v = nullptr);

private:
    int argc;
    char** argv;
    /* for `r3` checks */
    void checkName(int c, char** v);
};

}; // namespace PitchShifting