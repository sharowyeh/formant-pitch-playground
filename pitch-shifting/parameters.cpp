#include "parameters.h"

/* for given command options parser */
#ifdef _MSC_VER
// for win32, original getopt source code uses unsecure c runtime, workaround applied
#include "../getopt/getopt.h"
#else
#include <getopt.h>
#endif

// for param check or print usage
#include "helper.hpp"
/* to show version */
#include "rubberband/RubberBandStretcher.h"

using std::cerr;
using std::endl;

namespace PitchShifting {

Parameters::Parameters(int c, char** v): argc(c), argv(v)
{
    checkName(c, v);
}

void Parameters::checkName(int c, char** v)
{
    if (c == 0 || v == nullptr)
        return;
    myName = std::string(v[0]);
    isR3 =
        ((myName.size() > 3 &&
            myName.substr(myName.size() - 3, 3) == "-r3") ||
        (myName.size() > 7 &&
            myName.substr(myName.size() - 7, 7) == "-r3.exe") ||
        (myName.size() > 7 &&
            myName.substr(myName.size() - 7, 7) == "-R3.EXE"));
}

int Parameters::ParseOptions(int c, char** v)
{
    if (c) argc = c;
    if (v) argv = v;
    checkName(c, v);

    while (1) {
        int optionIndex = 0;
        // full, has_arg, p_flag, val
        static struct option longOpts[] = {
            { "help",          0, 0, 'h' },
            { "full-help",     0, 0, 'H' },
            { "version",       0, 0, 'V' },
            { "time",          1, 0, 't' },
            { "tempo",         1, 0, 'T' },
            { "duration",      1, 0, 'D' },
            { "pitch",         1, 0, 'p' },
            { "frequency",     1, 0, 'f' },
            { "crisp",         1, 0, 'c' },
            { "crispness",     1, 0, 'c' },
            { "debug",         1, 0, 'd' },
            { "realtime",      0, 0, 'R' },
            { "loose",         0, 0, 'L' },
            { "precise",       0, 0, 'P' },
            { "formant",       1, 0, 'F' },
            { "no-threads",    0, 0, '0' },
            { "no-transients", 0, 0, '1' },
            { "no-lamination", 0, 0, '.' },
            { "centre-focus",  0, 0, '7' },
            { "window-long",   0, 0, '>' },
            { "window-short",  0, 0, '<' },
            { "bl-transients", 0, 0, '8' },
            { "detector-perc", 0, 0, '5' },
            { "detector-soft", 0, 0, '6' },
            { "smoothing",     0, 0, '9' },
            { "pitch-hq",      0, 0, '%' },
            { "threads",       0, 0, '@' },
            { "quiet",         0, 0, 'q' },
            { "timemap",       1, 0, 'M' },
            { "freqmap",       1, 0, 'Q' },
            { "pitchmap",      1, 0, 'C' },
            { "ignore-clipping", 0, 0, 'i' },
            { "fast",          0, 0, '2' },
            { "fine",          0, 0, '3' },
            { "list-device",   0, 0, 'l' },
            { "input-gain",    1, 0, 'g' },
            { "gui",           0, 0, 'U' },
            { 0, 0, 0, 0 }
        };

        int optionChar = getopt_long(argc, argv,
            "t:p:d:RLPFc:f:T:D:qhHVM:23",
            longOpts, &optionIndex);
        if (optionChar == -1) break;

#ifdef __APPLE_CC__
        // getopt behavior is different in Xcode sdk? likes --pitch=9
        cerr << "ch:" << (char)optionChar << " idx:" << optionIndex << " opt:" << longOpts[optionIndex].name;
        cerr << " ind:" << optind << " arg:" << ((optarg && strlen(optarg) > 0) ? optarg : "NULL") << endl;
#endif

        switch (optionChar) {
        case 'h': help = true; break;
        case 'H': fullHelp = true; break;
        case 'V': version = true; break;
        case 't': timeratio *= atof(optarg); haveRatio = true; break;
        case 'T': timeratio *= tempo_convert(optarg); haveRatio = true; break;
        case 'D': duration = atof(optarg); haveRatio = true; break;
        case 'p': pitchshift = atof(optarg); haveRatio = true; break;
        case 'f': frequencyshift = atof(optarg); haveRatio = true; break;
        case 'd': debug = atoi(optarg); break;
        case 'R': realtime = true; break;
        case 'L': precisiongiven = true; break;
        case 'P': precisiongiven = true; break;
        case 'F': formantshift = atof(optarg); formant = true; break;
        case '0': threading = 1; break;
        case '@': threading = 2; break;
        case '1': transients = 0/*NoTransients*/; crispchanged = true; break;
        case '.': lamination = false; crispchanged = true; break;
        case '>': longwin = true; crispchanged = true; break;
        case '<': shortwin = true; crispchanged = true; break;
        case '5': detector = 1/*PercussiveDetector*/; crispchanged = true; break;
        case '6': detector = 2/*SoftDetector*/; crispchanged = true; break;
        case '7': together = true; break;
        case '8': transients = 1/*BandLimitedTransients*/; crispchanged = true; break;
        case '9': smoothing = true; crispchanged = true; break;
        case '%': hqpitch = true; break;
        case 'c': crispness = atoi(optarg); break;
        case 'q': quiet = true; break;
        case 'M': timeMapFile = optarg; break;
        case 'Q': freqMapFile = optarg; freqOrPitchMapSpecified = true; break;
        case 'C': pitchMapFile = optarg; freqOrPitchMapSpecified = true; break;
        case 'i': ignoreClipping = true; break;
        case '2': faster = true; break;
        case '3': finer = true; break;
        case 'l': listdev = true; break;
        case 'g': inputgaindb = atof(optarg); break;
        case 'U': gui = true; break;
        default:  help = true; break;
        }
    }

    if (version) {
        cerr << RUBBERBAND_VERSION << endl;
        return 0;
    }

    if (freqOrPitchMapSpecified) {
        if (freqMapFile != "" && pitchMapFile != "") {
            cerr << "ERROR: Please specify either pitch map or frequency map, not both" << endl;
            return 1;
        }
        haveRatio = true;
        realtime = true;
    }

    // at least given input wav file
    if (argc - optind >= 1) {
        inAudioParam = strdup(argv[optind]);
    }
    else {
        inAudioParam = (char*)malloc(sizeof(char) * 16);
#ifdef _WIN32
        strcpy_s(inAudioParam, 16, "test.wav");
#else
        strcpy(inAudioParam, "test.wav");
#endif
        argc++;
    }
    // given both input and output wav files
    if (argc - optind >= 2) {
        outAudioParam = strdup(argv[optind + 1]);
    }
    else {
        outAudioParam = (char*)malloc(sizeof(char) * 16);
#ifdef _WIN32
        strcpy_s(outAudioParam, 16, "test_out.wav");
#else
        strcpy(outAudioParam, "test.out.wav");
#endif
        argc++;
    }

    // resolve arguments
    ResolveArguments();

    // given parameters must contain input and output wav files
    if (help || fullHelp || !haveRatio || optind + 2 != argc) {
        print_usage(fullHelp, isR3, myName);
        return 2;
    }

    if (timeratio <= 0.0) {
        cerr << "ERROR: Invalid time ratio " << timeratio << endl;
        return 1;
    }

    if (faster && finer) {
        cerr << "WARNING: Both fast (R2) and fine (R3) engines selected, will use default for" << endl;
        cerr << "         this tool (" << (isR3 ? "fine" : "fast") << ")" << endl;
        faster = false;
        finer = false;
    }

    if (isR3) {
        if (!faster) {
            finer = true;
        }
    }
    else {
        if (!finer) {
            faster = true;
        }
    }

    typewin = (shortwin) ? 1 : (longwin) ? 2 : 0/*standard*/;

    if (!quiet) {
        if (finer) {
            if (shortwin) {
                cerr << "Using intermediate R3 (finer) single-windowed engine" << endl;
            }
            else {
                cerr << "Using R3 (finer) engine" << endl;
            }
        }
        else {
            cerr << "Using R2 (faster) engine" << endl;
            cerr << "Using crispness level: " << crispness << " (";
            switch (crispness) {
            case 0: cerr << "Mushy"; break;
            case 1: cerr << "Piano"; break;
            case 2: cerr << "Smooth"; break;
            case 3: cerr << "Balanced multitimbral mixture"; break;
            case 4: cerr << "Unpitched percussion with stable notes"; break;
            case 5: cerr << "Crisp monophonic instrumental"; break;
            case 6: cerr << "Unpitched solo percussion"; break;
            }
            cerr << ")" << endl;
        }
    }
    // because return code 0~2 represent leave process 
    return -1;
}

int Parameters::ResolveArguments()
{
    if (checkNumuric(inAudioParam, &inDeviceIdx)) {
        inAudioType = 2;// SourceType::AudioDevice
    }
    else {
        // original rubberband sample separate file extension code block...
        for (int i = strlen(inAudioParam); i > 0; ) {
            if (inAudioParam[--i] == '.') {
                inFileExt = inAudioParam + i + 1;
                inFilePath = std::string(inAudioParam);
                inAudioType = 1;// SourceType::AudioFile
                break;
            }
        }
    }
    if (checkNumuric(outAudioParam, &outDeviceIdx)) {
        outAudioType = 2;// SourceType::AudioDevice
    }
    else {
        for (int i = strlen(outAudioParam); i > 0; ) {
            if (outAudioParam[--i] == '.') {
                outFileExt = outAudioParam + i + 1;
                outFilePath = std::string(outAudioParam);
                outAudioType = 1;// SourceType::AudioFile;
                break;
            }
        }
    }
    return 0;
}

}