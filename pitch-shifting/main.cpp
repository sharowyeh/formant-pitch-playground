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

// to get to know further rubberband source code details, wonder to participate with 
// rubberband-library visual studio project, but still has more glitch than meson build
// (should be the compiler options issue...)
#if _DEBUG
// build by meson
#pragma comment(lib, "rubberband.lib")
#else
// build by visual studio project
#pragma comment(lib, "rubberband-library.lib")
#endif

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
#ifdef _MSC_VER
// sysutils.h defined extern methods for win32, but meson/ninja not compile cpp correctly with cl.exe, so import cpp here again
#include "src/common/sysutils.cpp"
#else
#include "src/common/sysutils.h"
#include "src/common/Profiler.h"
#endif

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

// for param check or print usage
#include "helper.hpp"

// for user input event to break rubberband process while loop
// TODO: since the original rubberband example using iostream to output debugging info,
//       it's not ideal way to integrate with curses c library for user input to stop while loop,
//       do something else... 
//#include <ncurses.h>

// for opengl gui
#include "Window.hpp"
#include "TimeoutPopup.h"
#include "Waveform.h"

GLUI::Window* window = nullptr;
GLUI::TimeoutPopup* leavePopup = nullptr;
GLUI::Waveform* waveform = nullptr;

int main(int argc, char **argv)
{
    //DEBUG: try init gui window here
    window = GLUI::Window::Create("Rubberband GUI", 1280, 720);
    window->OnRenderFrame = [](GLUI::Window* wnd) {
        //try here
    };
    window->OnWindowClosing = [](GLUI::Window* wnd) {
        printf("close!\n");
        // raise event to timeout popup
        leavePopup->Show(true, 3.f);
        // reset main window close flag, let main window close popup decide
        glfwSetWindowShouldClose(wnd->GetGlfwWindow(), 0);
    };
    leavePopup = new GLUI::TimeoutPopup(window->GetGlfwWindow());
    leavePopup->OnTimeoutElasped = [](GLFWwindow* wnd) {
        printf("timeout!\n");
    };
    waveform = new GLUI::Waveform();
    // DEBUG: read my debug audio
    waveform->LoadAudioFile("debug.wav");
    while (!window->PrepareFrame()) {
        // just because want curvy corner, must pair with PopStyleVar() restore style changes for rendering loop
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.f);
        leavePopup->Render();
        waveform->Update();
        ImGui::PopStyleVar();
        window->SwapWindow();
    }
    window->Destroy();
    //TODO: still need imgui init, place in window class?
    //glfwWindowInit();

    double ratio = 1.0;
    double duration = 0.0;
    double pitchshift = 0.0;
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
    
    char* inAudioParam;
    char* outAudioParam;

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
    const int defBlockSize = 1024;
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
    /*char *inAudioParam = strdup(argv[optind++]);
    char *outAudioParam = strdup(argv[optind++]);*/

    bool inUseFile = false;
    bool outUseFile = false;
    std::string extIn, extOut;
    for (int i = strlen(inAudioParam); i > 0; ) {
        if (inAudioParam[--i] == '.') {
            extIn = inAudioParam + i + 1;
            inUseFile = true;
            break;
        }
    }
    for (int i = strlen(outAudioParam); i > 0; ) {
        if (outAudioParam[--i] == '.') {
            extOut = outAudioParam + i + 1;
            outUseFile = true;
            break;
        }
    }

    int sampleRate = 0;
    int channels = 0;
    int format = 0;
    int64_t inputFrames = 0;

    // check input/output audio file or device
    bool checkAudio = true;
    if (inUseFile) {
        checkAudio = sther->LoadInputFile(inAudioParam, &sampleRate, &channels, &format, &inputFrames, ratio, duration);
    }
    if (outUseFile) {
        checkAudio = sther->SetOutputFile(outAudioParam, sampleRate, 2, format);
    }
    if (checkAudio == false) {
        cerr << "Set input/output to files but invalid" << endl;
        delete sther;
        return 1;
    }

    sther->ListAudioDevices();
    // list audio device only
    if (listdev) {
        delete sther;
        return 0;
    }

    // 0/2: mymacin48k, 9: mywinin44k(mme), 43: mywinin48k
    int inDevIndex = 43;
    // 1: mymacout48k, 15: mywinout44k(mme), 26/28/34: mywinout48k(wdm/aux/vcable)
    int outDevIndex = 26;
    bool inUseDev = checkNumuric(inAudioParam, &inDevIndex);
    bool outUseDev = checkNumuric(outAudioParam, &outDevIndex);

    if (inUseDev) {
        checkAudio = sther->SetInputStream(inDevIndex, &sampleRate, &channels);
    }
    if (outUseDev) {
        checkAudio = sther->SetOutputStream(outDevIndex);
    }
    if (checkAudio == false) {
        cerr << "Set input/output audio device failed" << endl;
        delete sther;
        return 1;
    }

    //DEBUG: TODO: currently use input frames for realtime duration
    inputFrames = sampleRate * 3600 * 3; // 3hr for long duration test 

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
    if (formantshift != 0.0) {
        cerr << "Formant shift semitones: " << formantshift << endl;
    }
    double formantFactor = pow(2.0, formantshift / 12.0);

    // default formant scale = 1.0 / freq(pitch)shift if formant enabled
    double formantScale = 0.0;
    if (formant) {
        // NOTE: pitch changes also affact formant scaling
        formantScale = 1.0 / frequencyshift;
        cerr << "Formant preserved default " << formantScale << "(from pitch shift)" << endl;
        formantScale *= formantFactor;
        cerr << "Formant factor " << formantFactor << ", formant scale " << formantScale << endl;
    }
    
    // apply gain, voltage level for audio signal will be pow(10.f, db / 20.f)
    if (inputgaindb != 0.0) {
        cerr << "Input gain db: " << inputgaindb << endl;
        double inputgainlv = pow(10.f, inputgaindb / 20.f);
        cerr << "Input gain level: " << inputgainlv << endl;
        sther->SetInputGain(inputgainlv);
    }

#ifdef _WIN32
    RubberBand::
#endif
    timeval tv;
    (void)gettimeofday(&tv, 0);

    size_t countIn = 0, countOut = 0;

    bool successful = false;
    int thisBlockSize;
    
    // initscr(); // initialize ncurses to get user input
    // noecho();
    // nodelay(stdscr, TRUE);

    //DEBUG: start GUI frame refresh here, NOTE: thread will be blocked and callback invokes depends on GUI refresh rate
    auto windowRenderCallback = [](void* param) {
    };
    //TODO: move to while loop, but it may be better thread isolated
    //glfwWindowRender(windowRenderCallback);
    //glfwWindowDestory();
    
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
            //if (getch() == 'q') {
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

    //endwin(); // cleanup ncurses

    sther->CloseFiles();

    sther->WaitStream();
    sther->StopInputStream();
    sther->StopOutputStream();

    free(inAudioParam);
    free(outAudioParam);

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

    //RubberBand::Profiler::dump();
    
    delete sther;

    return 0;
}


