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
// for usleep from unistd
#include <windows.h>
static void usleep(unsigned long usec) {
    ::Sleep(usec == 0 ? 0 : usec < 1000 ? 1 : usec / 1000);
}
#else
#include <unistd.h>
#include <sys/time.h>
#endif

#ifdef _WIN32
using RubberBand::gettimeofday;
#endif

using std::cerr;
using std::endl;
// to check char array text is numeric
#include "helper.hpp"
//
#include "parameters.h"

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

//TODO: not integrate to main yet
void debugGLwindow()
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
}

int main(int argc, char **argv)
{
    auto p = new PitchShifting::Parameters(argc, argv);
    auto code = p->ParseOptions();
    if (code >= 0) return code;
    //TODO: based on parameters packed standalone given p to sther?
    auto crispness = p->crispness;
    auto typewin = p->typewin;
    auto lamination = p->lamination;
    auto transients = p->transients;
    auto detector = p->detector;
    auto timeMapFile = p->timeMapFile;
    auto freqMapFile = p->freqMapFile;
    auto pitchMapFile = p->pitchMapFile;
    auto inAudioParam = p->inAudioParam;
    auto outAudioParam = p->outAudioParam;
    auto ratio = p->ratio;
    auto duration = p->duration;
    auto listdev = p->listdev;
    auto finer = p->finer;
    auto realtime = p->realtime;
    auto smoothing = p->smoothing;
    auto formant = p->formant;
    auto together = p->together;
    auto hqpitch = p->hqpitch;
    auto threading = p->threading;
    auto pitchshift = p->pitchshift;
    auto frequencyshift = p->frequencyshift;
    auto freqOrPitchMapSpecified = p->freqOrPitchMapSpecified;
    auto formantshift = p->formantshift;
    auto inputgaindb = p->inputgaindb;
    auto ignoreClipping = p->ignoreClipping;
    auto quiet = p->quiet;

    // start stretcher class initialization here
    const int defBlockSize = 1024;
    PitchShifting::Stretcher *sther = new PitchShifting::Stretcher(defBlockSize, 1);

    if (crispness != -1) {
        sther->SetCrispness(crispness, &typewin, &lamination, &transients, &detector);
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


