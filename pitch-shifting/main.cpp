/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/* source code from rubberband repo, modify its demo console app as alt project */
/* NOTE: based on rubberband features have been moved to stretcher class, also move lib dependency to stretcher.hpp */

#include <iostream>
#include <cmath>
#include <time.h>
#include <cstdlib>
#include <cstring>
#include <string>

#include "stretcher.hpp"
using PitchShifting::SourceType;
using PitchShifting::SourceDesc;

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

// for copy_if to select GUI ease of use datasets
#include <algorithm>
// for opengl gui
#include "Window.hpp"
#include "CtrlForm.h"
#include "TimeoutPopup.h"
#include "Waveform.h"
#include "RealTimePlot.h"
#include "ScalePlot.h"

GLUI::Window* window = nullptr;
GLUI::CtrlForm* ctrlForm = nullptr;
GLUI::TimeoutPopup* leavePopup = nullptr;
GLUI::Waveform* fileWaveform = nullptr;
GLUI::RealTimePlot* inWaveform = nullptr;
GLUI::RealTimePlot* outWaveform = nullptr;
GLUI::ScalePlot* formantChart = nullptr;
GLUI::ScalePlot* scaleChart = nullptr;
//std::vector<GLUI::ScalePlot*> scaleCharts;

std::thread* uiThread = nullptr;
/* design states for main thread communication, refer to FnWindowStates */
std::atomic<int> uiWindowState = 0;
/* design button starting flag when gui accquire stretcher processing */
std::atomic<bool> uiStartEnabled = false;
/* for window life cycle */
typedef void (*fnWindowStateChanged)(int);
/* for ctrl form button action */
typedef void (*fnButtonClicked)(std::string, int);
enum FnWindowStates :int {
    NONE,
    INITIALIZED, // ready to frame rendering
    CLOSING,
    CLOSED,
    RENDERING_FINISHED,
    DESTROYED
};
enum FnCallbackIds :int {
    ON_WINDOW_STATE_CHANGED,
    ON_BUTTON_CLICKED
};
std::map<int, void*> uiCallbackFnMap;

void uiThreadRun(const std::map<int, void*>& cbFnMapRef, PitchShifting::Parameters* paramPtr)
{
    cerr << "UI thread debug level: " << paramPtr->debug << endl;
    //DEBUG: singleton window instance
    window = GLUI::Window::Create("Rubberband GUI", 1366, 768);
    window->OnRenderFrame = [](GLUI::Window* wnd) {
        // is the same afterward the window->PrepareFrame()
    };
    //TODO: design fn map in a class to pass through parent cb in window callback lambda, like [this](wnd) {...}
    window->OnWindowClosing = [](GLUI::Window* wnd) {
        printf("on window close!\n");
        if (leavePopup) {
            // prevent main window closing from default behavior, hand over to popup callback -> timeout slapsed
            glfwSetWindowShouldClose(window->GetGlfwWindow(), 0);
            // raise timeout popup displaying time remaining
            leavePopup->Show(true, 3.f);
        }
    };
    ctrlForm = new GLUI::CtrlForm(window->GetGlfwWindow());
    ctrlForm->OnButtonClicked = (fnButtonClicked(cbFnMapRef.at(ON_BUTTON_CLICKED)));
    leavePopup = new GLUI::TimeoutPopup(window->GetGlfwWindow());
    leavePopup->OnTimeoutElapsed = [](GLFWwindow* wnd) {
        printf("on timeout elapsed!\n");
        // exit message loop to close main window
        glfwSetWindowShouldClose(window->GetGlfwWindow(), 1);
    };
    fileWaveform = new GLUI::Waveform("");
    inWaveform = new GLUI::RealTimePlot("in");
    outWaveform = new GLUI::RealTimePlot("out");
    formantChart = new GLUI::ScalePlot("formant");
    scaleChart = new GLUI::ScalePlot("classy"); // use formant fft size 2048

    // DEBUG: read my debug audio
    fileWaveform->LoadAudioFile("debug.wav");
    uiWindowState = INITIALIZED;
    if (cbFnMapRef.at(ON_WINDOW_STATE_CHANGED)) // can't use [] because it's const
        ((fnWindowStateChanged)cbFnMapRef.at(ON_WINDOW_STATE_CHANGED))(INITIALIZED);
    while (!window->PrepareFrame()) {
        // just because want curvy corner, must pair with PopStyleVar() restore style changes for rendering loop
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.f);

        ctrlForm->Render();
        leavePopup->Render();
        fileWaveform->Update();
        inWaveform->Update();
        outWaveform->Update();
        formantChart->Update();
        scaleChart->Update();
        
        ImGui::PopStyleVar();

        //ImPlot::ShowDemoWindow();
        
        window->SwapWindow();
    }
    uiWindowState = RENDERING_FINISHED;
    if (cbFnMapRef.at(ON_WINDOW_STATE_CHANGED))
        ((fnWindowStateChanged)cbFnMapRef.at(ON_WINDOW_STATE_CHANGED))(RENDERING_FINISHED);
    window->Destroy();
    uiWindowState = DESTROYED;
    if (cbFnMapRef.at(ON_WINDOW_STATE_CHANGED))
        ((fnWindowStateChanged)cbFnMapRef.at(ON_WINDOW_STATE_CHANGED))(DESTROYED);
}

void onWindowStateChanged(int state) {
    cerr << "Window state: " << state << endl;
}

void onButtonClicked(std::string name, int param) {
    cerr << "Button: " << name.c_str() << " param: " << param << endl;
    uiStartEnabled = (param == 1) ? true: false;
}

void setGLWindow(PitchShifting::Parameters* param)
{
    // only run once
    if (uiThread) return;
    // init callback function pointers
    uiCallbackFnMap.clear();
    uiCallbackFnMap[ON_WINDOW_STATE_CHANGED] = &onWindowStateChanged;
    uiCallbackFnMap[ON_BUTTON_CLICKED] = &onButtonClicked;
    // UI thread reporting GUI states
    uiWindowState = NONE;
    uiStartEnabled = false;
    auto bound = std::bind(uiThreadRun, uiCallbackFnMap, param);
    uiThread = new std::thread(bound);
    //DEBUG: it still better join the thread to fully control gui life cycle
    uiThread->detach();
}

void mapDataPtrToGuiPlot(PitchShifting::Stretcher* sther) {

    auto ptr_of_shared_ptr = sther->GetChannelData();
    auto formantFFTSize = sther->GetFormantFFTSize();
    auto scaleSizes = sther->GetChannelScaleSizes(0);
    std::cout << "got channel data: formant fft size:" << formantFFTSize << " scale size count:" << scaleSizes << std::endl;
    int bufSize = 0;
    double* dataPtr = nullptr;
    // using scale plot chart drawing formant data
    sther->GetFormantData(PitchShifting::Stretcher::FormantDataType::Cepstra, 0, &formantFFTSize, &dataPtr, &bufSize);
    formantChart->SetPlotInfo("Ceps", 1, 0, formantFFTSize, dataPtr, bufSize);
    sther->GetFormantData(PitchShifting::Stretcher::FormantDataType::Envelope, 0, &formantFFTSize, &dataPtr, &bufSize);
    formantChart->SetPlotInfo("Envelop", 2, 0, formantFFTSize, dataPtr, bufSize);
    sther->GetFormantData(PitchShifting::Stretcher::FormantDataType::Spare, 0, &formantFFTSize, &dataPtr, &bufSize);
    formantChart->SetPlotInfo("Spare", 3, 0, formantFFTSize, dataPtr, bufSize);
    int fftSize = formantFFTSize; // scale data just using formant fft size 2048
    //int fftSize = pow(2, (i + 10)); // 1024, 2048, 4096
    sther->GetChannelScaleData(PitchShifting::Stretcher::ScaleDataType::Real, 0, fftSize, &dataPtr, &bufSize); /* resolution 100 */
    scaleChart->SetPlotInfo("Real", 1, 0, fftSize, dataPtr, bufSize);
    sther->GetChannelScaleData(PitchShifting::Stretcher::ScaleDataType::Imaginary, 0, fftSize, &dataPtr, &bufSize);
    scaleChart->SetPlotInfo("Imag", 2, 0, fftSize, dataPtr, bufSize);
    sther->GetChannelScaleData(PitchShifting::Stretcher::ScaleDataType::Magnitude, 0, fftSize, &dataPtr, &bufSize); /* resolution 0.1 */
    scaleChart->SetPlotInfo("Mag", 3, 0, fftSize, dataPtr, bufSize);
    sther->GetChannelScaleData(PitchShifting::Stretcher::ScaleDataType::PreviousMagnitude, 0, fftSize, &dataPtr, &bufSize);
    scaleChart->SetPlotInfo("PrevMag", 6, 0, fftSize, dataPtr, bufSize);
    sther->GetChannelScaleData(PitchShifting::Stretcher::ScaleDataType::Phase, 0, fftSize, &dataPtr, &bufSize); /* resolution pi */
    scaleChart->SetPlotInfo("Phase", 4, 0, fftSize, dataPtr, bufSize);
    sther->GetChannelScaleData(PitchShifting::Stretcher::ScaleDataType::AdvancedPhase, 0, fftSize, &dataPtr, &bufSize);
    scaleChart->SetPlotInfo("AdPhase", 5, 0, fftSize, dataPtr, bufSize);
    sther->GetChannelScaleData(PitchShifting::Stretcher::ScaleDataType::PendingKick, 0, fftSize, &dataPtr, &bufSize); /* so far zero values */
    scaleChart->SetPlotInfo("Kick", 7, 0, fftSize, dataPtr, bufSize);
    sther->GetChannelScaleData(PitchShifting::Stretcher::ScaleDataType::Accumulator, 0, fftSize, &dataPtr, &bufSize); /* resolution 0.1 */
    scaleChart->SetPlotInfo("Accu", 8, 0, fftSize, dataPtr, bufSize);
}

int main(int argc, char **argv)
{
    PitchShifting::Parameters param;
    auto code = param.ParseOptions(argc, argv);
    if (code >= 0) return code;

    //DEBUG: force use gui for gui development
    param.gui = true;
    if (param.gui) {
        setGLWindow(&param);
    }
    
    // start stretcher class initialization here
    const int defBlockSize = 1024;
    PitchShifting::Stretcher *sther = new PitchShifting::Stretcher(&param, defBlockSize, 1);

    sther->LoadTimeMap(param.timeMapFile);
    bool pitchToFreq = param.freqMapFile.empty();
    sther->LoadFreqMap((pitchToFreq ? param.pitchMapFile : param.freqMapFile), pitchToFreq);

    SourceType inSource = SourceType::Unknown;
    SourceType outSource = SourceType::Unknown;
    std::string extIn, extOut;
    for (int i = strlen(param.inAudioParam); i > 0; ) {
        if (param.inAudioParam[--i] == '.') {
            extIn = param.inAudioParam + i + 1;
            inSource = SourceType::AudioFile;
            break;
        }
    }
    for (int i = strlen(param.outAudioParam); i > 0; ) {
        if (param.outAudioParam[--i] == '.') {
            extOut = param.outAudioParam + i + 1;
            outSource = SourceType::AudioFile;
            break;
        }
    }

    int sampleRate = 0;
    int channels = 0;
    int format = 0;
    int64_t inputFrames = 0;

    // check input/output audio file or device
    bool checkAudio = true;
    if (inSource) {
        checkAudio = sther->LoadInputFile(param.inAudioParam, &sampleRate, &channels, &format, &inputFrames, param.timeratio, param.duration);
    }
    if (outSource) {
        checkAudio &= sther->SetOutputFile(param.outAudioParam, sampleRate, 2, format);
    }
    if (checkAudio == false) {
        cerr << "Set input/output to files but invalid" << endl;
        delete sther;
        return 1;
    }

    std::vector<SourceDesc> devices;
    int devCount = sther->ListAudioDevices(devices);
    // list audio device only
    if (param.listdev) {
        delete sther;
        return 0;
    }

    // 0/2: mymacin48k, 9: mywinin44k(mme), 43: mywinin48k
    int inDevIndex = 43;
    // 1: mymacout48k, 15: mywinout44k(mme), 26/28/34: mywinout48k(wdm/aux/vcable)
    int outDevIndex = 26;
    if (checkNumuric(param.inAudioParam, &inDevIndex)) {
        checkAudio = sther->SetInputStream(inDevIndex, &sampleRate, &channels);
        if (checkAudio) {
            inSource = SourceType::AudioDevice;
        }
        else {
            inDevIndex = -1;
        }
    }
    if (checkNumuric(param.outAudioParam, &outDevIndex)) {
        checkAudio &= sther->SetOutputStream(outDevIndex);
        if (checkAudio) {
            outSource = SourceType::AudioDevice;
        }
        else {
            outDevIndex = -1;
        }
    }

    if (param.gui) {
        // append list to control form, and defualt selection from cli options
        ctrlForm->SetAudioDeviceList(devices, inDevIndex, outDevIndex);
    }
    
    if (checkAudio == false) {
        if (param.gui == false) {
        // leave app if no available audio sources in console mode
            cerr << "Set input/output audio device failed" << endl;
            delete sther;
            return 1;
        }
        else {
            // wait button event(from button clicked callback) after audio device selection in gui mode
            while (uiStartEnabled == false) {
                Sleep(100);
                if (uiWindowState == FnWindowStates::DESTROYED) {
                    cerr << "CLI given in/out source failed and GUI closed" << endl;
                    delete sther;
                    return 1;
                }
            }
            //TODO: get selection and re-check audio device can be opened
        }
    }

    // if use capture device as input, given duration or infinity -1?
    if (inSource == SourceType::AudioDevice) {
        inputFrames = (int)INFINITE;//sampleRate * 3600 * 3; // 3hr for long duration test 
    }

    //DEBUG: section for GUI initialization before stretcher creation(after ctor, but before rubber band configuration)
    if (param.gui) {
        // wait until all GUI classes constructed in another thread (mainly for file waveform loading file)
        while (uiWindowState != FnWindowStates::INITIALIZED) {
            Sleep(100);
        }

        // set audio information to GUI plot, given inFrame must afterward sther->SetInputStream for buffer initialization
        inWaveform->SetAudioInfo(sampleRate, channels, sther->inFrame, defBlockSize * channels);
        outWaveform->SetAudioInfo(
            sther->outInfo->defaultSampleRate,
            sther->outInfo->maxOutputChannels,
            sther->outFrame,
            defBlockSize * sther->outInfo->maxOutputChannels);
    }

    if (param.pitchshift != 0.0) {
        param.frequencyshift *= pow(2.0, param.pitchshift / 12.0);
        cerr << "Pitch shift semitones: " << param.pitchshift << endl;
    }

    cerr << "Using time ratio " << param.timeratio;

    if (!param.freqOrPitchMapSpecified) {
        cerr << " and frequency ratio " << param.frequencyshift << " for pitch shift" << endl;
    } else {
        cerr << " and initial frequency ratio " << param.frequencyshift << "for pitch shift" << endl;
    }

    // NOTE: formant adjustment only works to r3
    if (param.formantshift != 0.0) {
        cerr << "Formant shift semitones: " << param.formantshift << endl;
    }
    double formantShift = pow(2.0, param.formantshift / 12.0);

    // default formant scale = 1.0 / freq(pitch)shift if formant enabled
    double formantScale = 0.0;
    if (param.formant) {
        // NOTE: pitch changes also affact formant scaling
        formantScale = 1.0 / param.frequencyshift;
        cerr << "Formant preserved default " << formantScale << " by pitch shift" << endl;
        formantScale *= formantShift;
        cerr << "Formant ratio " << formantShift << ", results formant scale " << formantScale << endl;
    }
    
    // apply gain, voltage level for audio signal will be pow(10.f, db / 20.f)
    if (param.inputgaindb != 0.0) {
        cerr << "Input gain db: " << param.inputgaindb << endl;
        double inputgainlv = pow(10.f, param.inputgaindb / 20.f);
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
    
    setWaitKey('q'); // set wait key for user interrupts process loop (mainly to cancel audio stream realtime works)

    while (!successful) { // we may have to repeat with a modified
                          // gain, if clipping occurs
        successful = true;

        sther->Create(sampleRate, channels, param.timeratio, param.frequencyshift);
        
        /* DEBUG: playground with channel data */
        if (param.gui) {
            mapDataPtrToGuiPlot(sther);
        }
        
        if (inSource == SourceType::AudioFile) {
            sther->ExpectedInputDuration(inputFrames); // estimate from input file
        }
        sther->MaxProcessSize(defBlockSize);
        sther->FormantScale(formantScale);
        sther->SetIgnoreClipping(param.ignoreClipping);

        sther->StartInputStream();
        sther->StartOutputStream();

        if (!param.realtime) {
            sther->StudyInputSound(); // only works on input data source is file
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
            
            if (frame == 0 && !param.realtime && !param.quiet) {
                cerr << "Pass 2: Processing..." << endl;
            }

            // show process percentage if input source is audio file(estimatable duration)
            if (inSource == SourceType::AudioFile) {
                int p = int((double(frame) * 100.0) / inputFrames);
                if (p > percent || frame == 0) {
                    percent = p;
                    if (!param.quiet) {
                        cerr << "\r" << percent << "% ";
                    }
                }
            }
            
            // exit while loop if all input frames are processed
            if (frame >= inputFrames && inputFrames > 0) {
                cerr << "=== End reading inputs ===" << endl;
                // TODO: original design is frame + blockSize >= total input frames (forget why)
                reading = false;
            }
            // peaceful leaving while loop if user press any key to cancel
            if (isWaitKeyPressed()) {
                cerr << "=== Cancel reading inputs ===" << endl;
                reading = false;
                successful = true;
            }
            //DEBUG: check GUI window state if destroyed to interrupt processing
            if (param.gui && uiWindowState == FnWindowStates::DESTROYED) {
                cerr << "=== GUI was destroyed to leave ===" << endl;
                reading = false;
            }
        } // while (reading)

        if (!successful) {
            cerr << "WARNING: found clipping during process, decrease gain and process from begin again.." << endl;
            cerr << "         but I want to ignore this for realtime process.." << endl;
            if (!param.realtime) {
                // TODO: new functions to reopen or seek 0 to input/output files for
                //       re-entering while (!successful) loop
                //continue;
            }
            successful = true; // ignore clipping
        }
    
        if (!param.quiet) {
            cerr << "\r    " << endl;
        }

        // NOTE: get availble processed blocks from modified gain and clipping
        //       after nothing can read from input raised final=true until successful=true
        sther->RetrieveAvailableData(&countOut);

    } // while (successful)

    sther->CloseFiles();

    //sther->WaitStream(); // DEBUG: no need to wait stream for callback, use main loop instead
    sther->StopInputStream();
    sther->StopOutputStream();

    free(param.inAudioParam);
    free(param.outAudioParam);

    if (!param.quiet) {

        cerr << "in: " << countIn << ", out: " << countOut
             << ", ratio: " << float(countOut)/float(countIn)
             << ", ideal output: " << lrint(countIn * param.timeratio)
             << ", error: " << int(countOut) - lrint(countIn * param.timeratio)
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


