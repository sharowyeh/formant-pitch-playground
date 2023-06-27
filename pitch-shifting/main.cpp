/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band Library
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2023 Particular Programs Ltd.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.

    Alternatively, if you have a valid commercial licence for the
    Rubber Band Library obtained by agreement with the copyright
    holders, you may redistribute and/or modify it under the terms
    described in that licence.

    If you wish to distribute code using the Rubber Band Library
    under terms other than those of the GNU General Public License,
    you must obtain a valid commercial licence before doing so.
*/

// source code from rubberband repo, modify references for alt project

#include <iostream>
#include <cmath>
#include <time.h>
#include <cstdlib>
#include <cstring>
#include <string>

#include "stretcher.hpp"

// for rubberband profiler to dump signal process information,
// get rubberband source code from github within same system installed version
// and add the folder to project include path
#include "src/common/sysutils.h"
#include "src/common/Profiler.h"

#ifdef _MSC_VER
// for win32, original getopt source code uses unsecure c runtime, workaround applied
#include "../getopt/getopt.h"
// for usleep from unistd
#include <windows.h>
static void usleep(unsigned long usec) {
    ::Sleep(usec == 0 ? 0 : usec < 1000 ? 1 : usec / 1000);
}
#define strdup _strdup
#else
#include <getopt.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#ifdef _WIN32
using RubberBand::gettimeofday;
#endif

using std::cerr;
using std::endl;

double tempo_convert(const char *str)
{
    char *d = strchr((char *)str, ':');

    if (!d || !*d) {
        double m = atof(str);
        if (m != 0.0) return 1.0 / m;
        else return 1.0;
    }

    char *a = strdup(str);
    char *b = strdup(d+1);
    a[d-str] = '\0';
    double m = atof(a);
    double n = atof(b);
    free(a);
    free(b);
    if (n != 0.0 && m != 0.0) return m / n;
    else return 1.0;
}

void print_usage(bool fullHelp, bool isR3, std::string myName) {
    cerr << endl;
    cerr << "Rubber Band" << endl;
    cerr << "An audio time-stretching and pitch-shifting library and utility program." << endl;
    cerr << "Copyright 2007-2023 Particular Programs Ltd." << endl;
    cerr << endl;
    cerr << "   Usage: " << myName << " [options] <infile.wav> <outfile.wav>" << endl;
    cerr << endl;
    cerr << "You must specify at least one of the following time and pitch ratio options:" << endl;
    cerr << endl;
    cerr << "  -t<X>, --time <X>       Stretch to X times original duration, or" << endl;
    cerr << "  -T<X>, --tempo <X>      Change tempo by multiple X (same as --time 1/X), or" << endl;
    cerr << "  -T<X>, --tempo <X>:<Y>  Change tempo from X to Y (same as --time X/Y), or" << endl;
    cerr << "  -D<X>, --duration <X>   Stretch or squash to make output file X seconds long" << endl;
    cerr << endl;
    cerr << "  -p<X>, --pitch <X>      Raise pitch by X semitones, or" << endl;
    cerr << "  -f<X>, --frequency <X>  Change frequency by multiple X" << endl;
    cerr << endl;
    cerr << "The following options provide ways of making the time and frequency ratios" << endl;
    cerr << "change during the audio:" << endl;
    cerr << endl;
    cerr << "  -M<F>, --timemap <F>    Use file F as the source for time map" << endl;
    cerr << endl;
    cerr << "  A time map (or key-frame map) file contains a series of lines, each with two" << endl;
    cerr << "  sample frame numbers separated by a single space. These are source and" << endl;
    cerr << "  target frames for fixed time points within the audio data, defining a varying" << endl;
    cerr << "  stretch factor through the audio. When supplying a time map you must specify" << endl;
    cerr << "  an overall stretch factor using -t, -T, or -D as well, to determine the" << endl;
    cerr << "  total output duration." << endl;
    cerr << endl;
    cerr << "         --pitchmap <F>   Use file F as the source for pitch map" << endl;
    cerr << endl;
    cerr << "  A pitch map file contains a series of lines, each with two values: the input" << endl;
    cerr << "  sample frame number and a pitch offset in semitones, separated by a single" << endl;
    cerr << "  space. These specify a varying pitch factor through the audio. The offsets" << endl;
    cerr << "  are all relative to an initial offset specified by the pitch or frequency" << endl;
    cerr << "  option, or relative to no shift if neither was specified. Offsets are" << endl;
    cerr << "  not cumulative. This option implies realtime mode (-R) and also enables a" << endl;
    cerr << "  high-consistency pitch shifting mode, appropriate for dynamic pitch changes." << endl;
    cerr << "  Because of the use of realtime mode, the overall duration will not be exact." << endl;
    cerr << endl;
    cerr << "         --freqmap <F>    Use file F as the source for frequency map" << endl;
    cerr << endl;
    cerr << "  A frequency map file is like a pitch map, except that its second column" << endl;
    cerr << "  lists frequency multipliers rather than pitch offsets (like the difference" << endl;
    cerr << "  between pitch and frequency options above)." << endl;
    cerr << endl;
    cerr << "The following options affect the sound manipulation and quality:" << endl;
    cerr << endl;
    cerr << "  -2,    --fast           Use the R2 (faster) engine" << endl;
    cerr << endl;
    cerr << "  This is the default (for backward compatibility) when this tool is invoked" << endl;
    cerr << "  as \"rubberband\". It was the only engine available in versions prior to v3.0." << endl;
    cerr << endl;
    cerr << "  -3,    --fine           Use the R3 (finer) engine" << endl;
    cerr << endl;
    cerr << "  This is the default when this tool is invoked as \"rubberband-r3\". It almost" << endl;
    cerr << "  always produces better results than the R2 engine, but with significantly" << endl;
    cerr << "  higher CPU load." << endl;
    cerr << endl;
    cerr << "  -F,    --formant        Enable formant preservation when pitch shifting" << endl;
    cerr << endl;
    cerr << "  This option attempts to keep the formant envelope unchanged when changing" << endl;
    cerr << "  the pitch, retaining the original timbre of vocals and instruments in a" << endl;
    cerr << "  recognisable way." << endl;
    cerr << endl;
    cerr << "         --centre-focus   Preserve focus of centre material in stereo" << endl;
    cerr << endl;
    cerr << "  This option assumes that any 2-channel audio files are stereo and treats" << endl;
    cerr << "  them in a way that improves focus of the centre material at a small expense" << endl;
    cerr << "  in quality of the individual channels. In v3.2+ (and R2) this also" << endl;
    cerr << "  preserves mono compatibility, which the default options do not always." << endl;
    cerr << endl;
    if (fullHelp || !isR3) {
        cerr << "  -c<N>, --crisp <N>      Crispness (N = 0,1,2,3,4,5,6); default 5" << endl;
        cerr << endl;
        cerr << "  This option only has an effect when using the R2 (faster) engine. See" << endl;
        if (fullHelp) {
            cerr << "  below ";
        }
        else {
            cerr << "  the full help ";
        }
        cerr << "for details of the different levels." << endl;
        cerr << endl;
    }
    if (fullHelp) {
        cerr << "The remaining options fine-tune the processing mode and stretch algorithm." << endl;
        cerr << "The default is to use none of these options." << endl;
        cerr << "The options marked (2) currently only have an effect when using the R2 engine" << endl;
        cerr << "(see -2, -3 options above)." << endl;
        cerr << endl;
        cerr << "  -R,    --realtime       Select realtime mode (implies --no-threads)." << endl;
        cerr << "                          This utility does not do realtime stream processing;" << endl;
        cerr << "                          the option merely selects realtime mode for the" << endl;
        cerr << "                          stretcher it uses" << endl;
        cerr << "(2)      --no-threads     No extra threads regardless of CPU and channel count" << endl;
        cerr << "(2)      --threads        Assume multi-CPU even if only one CPU is identified" << endl;
        cerr << "(2)      --no-transients  Disable phase resynchronisation at transients" << endl;
        cerr << "(2)      --bl-transients  Band-limit phase resync to extreme frequencies" << endl;
        cerr << "(2)      --no-lamination  Disable phase lamination" << endl;
        cerr << "(2)      --smoothing      Apply window presum and time-domain smoothing" << endl;
        cerr << "(2)      --detector-perc  Use percussive transient detector (as in pre-1.5)" << endl;
        cerr << "(2)      --detector-soft  Use soft transient detector" << endl;
        cerr << "(2)      --window-long    Use longer processing window (actual size may vary)" << endl;
        cerr << "         --window-short   Use shorter processing window (with the R3 engine" << endl;
        cerr << "                          this is effectively a quick \"draft mode\")" << endl;
        cerr << "         --pitch-hq       In RT mode, use a slower, higher quality pitch shift" << endl;
        cerr << "         --ignore-clipping Ignore clipping at output; the default is to restart" << endl;
        cerr << "                          with reduced gain if clipping occurs" << endl;
        cerr << "  -L,    --loose          [Accepted for compatibility but ignored; always off]" << endl;
        cerr << "  -P,    --precise        [Accepted for compatibility but ignored; always on]" << endl;
        cerr << endl;
        cerr << "  -d<N>, --debug <N>      Select debug level (N = 0,1,2,3); default 0, full 3" << endl;
        cerr << "                          (N.B. debug level 3 includes audible ticks in output)" << endl;
        cerr << endl;
    }
    cerr << "The following options are for output control and administration:" << endl;
    cerr << endl;
    cerr << "  -q,    --quiet          Suppress progress output" << endl;
    cerr << "  -V,    --version        Show version number and exit" << endl;
    cerr << "  -h,    --help           Show the normal help output" << endl;
    cerr << "  -H,    --full-help      Show the full help output" << endl;
    cerr << endl;
    if (fullHelp) {
        cerr << "\"Crispness\" levels: (2)" << endl;
        cerr << "  -c 0   equivalent to --no-transients --no-lamination --window-long" << endl;
        cerr << "  -c 1   equivalent to --detector-soft --no-lamination --window-long (for piano)" << endl;
        cerr << "  -c 2   equivalent to --no-transients --no-lamination" << endl;
        cerr << "  -c 3   equivalent to --no-transients" << endl;
        cerr << "  -c 4   equivalent to --bl-transients" << endl;
        cerr << "  -c 5   default processing options" << endl;
        cerr << "  -c 6   equivalent to --no-lamination --window-short (may be good for drums)" << endl;
        cerr << endl;
    }
    else {
        cerr << "Numerous other options are available, mostly for tuning the behaviour of" << endl;
        cerr << "the R2 engine. Run \"" << myName << " --full-help\" for details." << endl;
        cerr << endl;
    }
}

int main(int argc, char **argv)
{
    double ratio = 1.0;
    double duration = 0.0;
    double pitchshift = 0.0;
    double frequencyshift = 1.0;
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

    bool haveRatio = false;

    std::string timeMapFile;
    std::string freqMapFile;
    std::string pitchMapFile;
    bool freqOrPitchMapSpecified = false;

    int transients = 2;/*Transients*/
    int detector = 0;/*CompoundDetector*/

    bool ignoreClipping = false;

    std::string myName(argv[0]);

    bool isR3 =
        ((myName.size() > 3 &&
          myName.substr(myName.size() - 3, 3) == "-r3") ||
         (myName.size() > 7 &&
          myName.substr(myName.size() - 7, 7) == "-r3.exe") ||
         (myName.size() > 7 &&
          myName.substr(myName.size() - 7, 7) == "-R3.EXE"));
    
    while (1) {
        int optionIndex = 0;

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
            { "formant",       0, 0, 'F' },
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
        case 't': ratio *= atof(optarg); haveRatio = true; break;
        case 'T': ratio *= tempo_convert(optarg); haveRatio = true; break;
        case 'D': duration = atof(optarg); haveRatio = true; break;
        case 'p': pitchshift = atof(optarg); haveRatio = true; break;
        case 'f': frequencyshift = atof(optarg); haveRatio = true; break;
        case 'd': debug = atoi(optarg); break;
        case 'R': realtime = true; break;
        case 'L': precisiongiven = true; break;
        case 'P': precisiongiven = true; break;
        case 'F': formant = true; break;
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
    
    char* fileName;
    char* fileNameOut;

    // at least given input wav file
    if (argc - optind >= 1) {
        fileName = strdup(argv[optind]);
    }
    else {
        fileName = (char*)malloc(sizeof(char) * 16);
#ifdef _WIN32
        strcpy_s(fileName, 16, "test.wav");
#else
        strcpy(fileName, "test.wav");
#endif
        argc++;
    }
    // given both input and output wav files
    if (argc - optind >= 2) {
        fileNameOut = strdup(argv[optind + 1]);
    }
    else {
        fileNameOut = (char*)malloc(sizeof(char) * 16);
#ifdef _WIN32
        strcpy_s(fileNameOut, 16, "test_out.wav");
#else
        strcpy(fileNameOut, "test.out.wav");
#endif
        argc++;
    }

    // given parameters must contain input and output wav files
    if (help || fullHelp || !haveRatio || optind + 2 != argc) {
        print_usage(fullHelp, isR3, myName);
        return 2;
    }

    if (ratio <= 0.0) {
        cerr << "ERROR: Invalid time ratio " << ratio << endl;
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
    } else {
        if (!finer) {
            faster = true;
        }
    }

    // TODO: we can start our stretcher here
    const int defBlockSize = 2048;
    PitchShifting::Stretcher *sther = new PitchShifting::Stretcher(defBlockSize, 1);

    int typewin = (shortwin) ? 1 : (longwin) ? 2 : 0/*standard*/;
    if (crispness != -1) {
        sther->SetCrispness(crispness, &typewin, &lamination, &transients, &detector);
    }

    if (!quiet) {
        if (finer) {
            if (shortwin) {
                cerr << "Using intermediate R3 (finer) single-windowed engine" << endl;
            } else {
                cerr << "Using R3 (finer) engine" << endl;
            }
        } else {
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

    sther->LoadTimeMap(timeMapFile);
    bool pitchToFreq = freqMapFile.empty();
    sther->LoadFreqMap((pitchToFreq ? pitchMapFile : freqMapFile), pitchToFreq);

    // move input/output file name right aftering getopt resolver
    /*char *fileName = strdup(argv[optind++]);
    char *fileNameOut = strdup(argv[optind++]);*/

    std::string extIn, extOut;
    for (int i = strlen(fileName); i > 0; ) {
        if (fileName[--i] == '.') {
            extIn = fileName + i + 1;
            break;
        }
    }
    for (int i = strlen(fileNameOut); i > 0; ) {
        if (fileNameOut[--i] == '.') {
            extOut = fileNameOut + i + 1;
            break;
        }
    }

    int sampleRate = 0;
    int channels = 0;
    int format = 0;
    int64_t inputFrames = 0;
    // check input/output audio file or device
    bool hasAudio = true;
    //hasAudio = sther->LoadInputFile(fileName, &sampleRate, &channels, &format, &inputFrames, ratio, duration);  
    if (hasAudio == false) {
        return 1;
    }
    // TODO: force align to input file, or use GetFileFormat()
    //sther->SetOutputFile(fileNameOut, sampleRate, 2, format);

	sther->ListAudioDevices();
	// 0/2: mymacin48k, 8: mywinin44k, 39: mywinin48k
	hasAudio = sther->SetInputStream(2, &sampleRate, &channels);
    if (hasAudio == false) {
        return 1;
    }
	inputFrames = sampleRate * 10; //DEBUG: 3s?
	// 1: mymacout48k, 14: mywinout44k, 24: mywinout48k
	hasAudio = sther->SetOutputStream(1);
    if (hasAudio == false) {
        return 1;
    }

    RubberBandStretcher::Options options = 0;
    options = sther->SetOptions(finer, realtime, typewin, smoothing, formant,
        together, hqpitch, lamination, threading, transients, detector);

    if (pitchshift != 0.0) {
        frequencyshift *= pow(2.0, pitchshift / 12.0);
    }

    cerr << "Using time ratio " << ratio;

    if (!freqOrPitchMapSpecified) {
        cerr << " and frequency ratio " << frequencyshift << endl;
    } else {
        cerr << " and initial frequency ratio " << frequencyshift << endl;
    }

    // NOTE: formant adjustment only works to r3
    //TODO: make option
    double formantshift = 3.0; // semitones
    double formantFactor = pow(2.0, formantshift / 12.0);

    // default formant scale = 1.0 / freq(pitch)shift if formant enabled
    double formantScale = 0.0;
    if (formant) {
        formantScale = 1.0 / frequencyshift;
        cerr << "Formant preserved default " << formantScale << endl;
        formantScale *= formantFactor;
        cerr << "Formant factor " << formantFactor << " results ratio " << formantScale << endl;
    }
    
#ifdef _WIN32
    RubberBand::
#endif
    timeval tv;
    (void)gettimeofday(&tv, 0);

    size_t countIn = 0, countOut = 0;

    bool successful = false;

    int thisBlockSize;

    while (!successful) { // we may have to repeat with a modified
                          // gain, if clipping occurs
        successful = true;

        sther->Create(sampleRate, channels, options, ratio, frequencyshift);
        
        //sther->ExpectedInputDuration(inputFrames); // estimate from input file
        sther->MaxProcessSize(defBlockSize);
        sther->FormantScale(formantScale);
        sther->SetIgnoreClipping(ignoreClipping);

		sther->StartInputStream();
		sther->StartOutputStream();

        if (!realtime) {
            sther->StudyInputSound();
        }

        int frame = 0;
        int percent = 0;

        sther->SetKeyFrameMap();

        countIn = 0;
        countOut = 0;
        bool clipping = false;

        // The stretcher only pads the start in offline mode; to avoid
        // a fade in at the start, we pad it manually in RT mode. Both
        // of these functions are defined to return zero in offline mode
        int toDrop = 0;
        toDrop = sther->ProcessStartPad();
        sther->SetDropFrames(toDrop);

        bool reading = true;
        //TODO: check here for realtime via portaudio
        while (reading) {

            thisBlockSize = defBlockSize;
            sther->ApplyFreqMap(countIn, &thisBlockSize);

            // TODO: given block size parameter if frequency map is enabled
			// TODO: change input function to device instead of file
            bool isFinal = sther->ProcessInputSound(&frame, &countIn);

            // TODO: may check result if clipping occurred to successful variable
            successful = sther->RetrieveAvailableData(&countOut, isFinal);
            //if (!successful) break;
            
            if (frame == 0 && !realtime && !quiet) {
                cerr << "Pass 2: Processing..." << endl;
            }

            // int p = int((double(frame) * 100.0) / inputFrames);
            // if (p > percent || frame == 0) {
            //     percent = p;
            //     if (!quiet) {
            //         cerr << "\r" << percent << "% ";
            //     }
            // }
            
            // exit while loop if all input frames are processed
            if (frame >= inputFrames) {
				cerr << "=== Stop reading inputs ===" << endl;
                // TODO: original design is frame + blockSize >= total input frames
                reading = false;
            }
        } // while (reading)

        if (!successful) {
            cerr << "WARNING: found clipping during process, decrease gain and process from begin again.." << endl;
            cerr << "         but I want to ignore this for realtime process.." << endl;
            if (!realtime) {
                // TODO: new functions to reopen or seek 0 to input/output files for
                //       re-entering while (!successful) loop
                //continue;
            }
            successful = true; // ignore clipping
        }
    
        if (!quiet) {
            cerr << "\r    " << endl;
        }

        // NOTE: get availble processed blocks from modified gain and clipping
        //       after nothing can read from input raised final=true until successful=true
        sther->RetrieveAvailableData(&countOut);

    } // while (successful)

    // delete[] ibuf;
    // for (size_t c = 0; c < channels; ++c) {
    //     delete[] cbuf[c];
    // }
    // delete[] cbuf;

    sther->CloseFiles();

	sther->WaitStream();
	sther->StopOutputStream();

    free(fileName);
    free(fileNameOut);

    if (!quiet) {

        cerr << "in: " << countIn << ", out: " << countOut
             << ", ratio: " << float(countOut)/float(countIn)
             << ", ideal output: " << lrint(countIn * ratio)
             << ", error: " << int(countOut) - lrint(countIn * ratio)
             << endl;

#ifdef _WIN32
        RubberBand::
#endif
            timeval etv;
        (void)gettimeofday(&etv, 0);
        
        etv.tv_sec -= tv.tv_sec;
        if (etv.tv_usec < tv.tv_usec) {
            etv.tv_usec += 1000000;
            etv.tv_sec -= 1;
        }
        etv.tv_usec -= tv.tv_usec;
        
        double sec = double(etv.tv_sec) + (double(etv.tv_usec) / 1000000.0);
        cerr << "elapsed time: " << sec << " sec, in frames/sec: "
             << int64_t(countIn/sec) << ", out frames/sec: "
             << int64_t(countOut/sec) << endl;
    }

    RubberBand::Profiler::dump();
    
    delete sther;

    return 0;
}


