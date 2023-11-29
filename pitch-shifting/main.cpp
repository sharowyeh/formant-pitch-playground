/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/* source code from rubberband repo, modify its demo console app as alt project */
/* NOTE: based on rubberband features have been moved to stretcher class, also move lib dependency to stretcher.hpp */

#include <iostream>
#include <cmath>
#include <time.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <limits>

#include "stretcher.hpp"
using PitchShifting::SourceType;
using PitchShifting::SourceDesc;
std::thread* stherThread = nullptr;

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
/* design button flag when gui accquire audio source changes */
std::atomic<int> uiSetAudioButton = 0;
std::atomic<GLUI::CtrlFormData*> uiCtrlFormData;
/* design button flag when gui accquire stretcher processing */
std::atomic<bool> uiStartStretcher = false;
/* for window life cycle */
typedef void (*fnWindowStateChanged)(int);
/* for ctrl form button action */
typedef void (*fnButtonClicked)(void*, GLUI::ButtonEventArgs);

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

void uiCreate(const std::map<int, void*>& cbFnMapRef, PitchShifting::Parameters* paramPtr)
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
    //fileWaveform->LoadAudioFile("debug.wav");

    // rising UIs are ready and starting to render frame
    if (uiWindowState == FnWindowStates::NONE) {
        uiWindowState = INITIALIZED;
        if (cbFnMapRef.at(ON_WINDOW_STATE_CHANGED)) // can't use [] because it's const
            ((fnWindowStateChanged)cbFnMapRef.at(ON_WINDOW_STATE_CHANGED))(INITIALIZED);
    }
}

int uiPrepareFrame() {
    int code = window->PrepareFrame();
    if (code > 0) return code;

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

    return 0;
}

void uiTerminate(const std::map<int, void*>& cbFnMapRef) {
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

void onButtonClicked(void* inst, GLUI::ButtonEventArgs param) {
    auto id = std::get<0>(param.data);
    auto arg = std::get<1>(param.data);
    cerr << "Button: " << id << " param: " << arg << endl;
    uiSetAudioButton.store(id);
    switch (id) {
    case GLUI::CtrlFormIds::SetInputDeviceButton:
    case GLUI::CtrlFormIds::SetOutputDeviceButton:
    case GLUI::CtrlFormIds::SetInputFileButton:
    case GLUI::CtrlFormIds::SetAdjustmentButton:
        uiCtrlFormData.store((GLUI::CtrlFormData*)arg);
        break;
    default:
        break;
    }
}

void setGLWindow(PitchShifting::Parameters* param)
{
    // only run once
    //if (uiThread) return;
    // init callback function pointers
    uiCallbackFnMap.clear();
    uiCallbackFnMap[ON_WINDOW_STATE_CHANGED] = (void*)onWindowStateChanged;
    uiCallbackFnMap[ON_BUTTON_CLICKED] = (void*)onButtonClicked;
    // UI thread reporting GUI states
    uiWindowState = NONE;
    uiCtrlFormData = nullptr;
    uiStartStretcher = false;
    //auto bound = std::bind([](const std::map<int, void*>& cbFnMapRef, PitchShifting::Parameters* paramPtr) {
    //    uiCreate(cbFnMapRef, paramPtr);
    //    while (uiPrepareFrame() == 0) {
    //    }
    //    uiTerminate(cbFnMapRef);
    //    }, uiCallbackFnMap, param);
    //uiThread = new std::thread(bound);
    ////DEBUG: it still better join the thread to fully control gui life cycle
    //uiThread->detach();
}

/* associate data pointer between rubberband stretcher data and GUI data present source */
void mapDataPtrToGuiPlot(PitchShifting::Stretcher* sther) {

    auto ptr_of_shared_ptr = sther->GetChannelData();
    auto formantFFTSize = sther->GetFormantFFTSize();
    auto scaleSizes = sther->GetChannelScaleSizes(0);
    std::cout << "got channel data: formant fft size:" << formantFFTSize << " scale size count:" << scaleSizes << std::endl;
    int bufSize = 0;
    double* dataPtr = nullptr;
    // TODO: draw pitch to realtime waveform, i guess prev mag is for input which store before change formant, or real?
    sther->GetChannelScaleData(PitchShifting::Stretcher::ScaleDataType::Real, 0, formantFFTSize, &dataPtr, &bufSize);
    inWaveform->SetPitchInfo(dataPtr, bufSize);
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

bool setAudioSource(PitchShifting::Parameters& param, PitchShifting::Stretcher* sther,
    int& sampleRate, int& channels, int& format, int64_t& inputFrames) {
    bool result = false;
    switch (param.inAudioType) {
    case SourceType::AudioFile:
        result = sther->LoadInputFile(param.inFilePath, &sampleRate, &channels, &format, &inputFrames, param.timeratio, param.duration);
        if (param.debug > 2) {
            cerr << "Load audio file format: " << format << ", ext format: " << sther->GetFileFormat(param.inFileExt) << endl;
        }
        break;
    case SourceType::AudioDevice:
        result = sther->SetInputStream(param.inDeviceIdx, &sampleRate, &channels);
        inputFrames = std::numeric_limits<int64_t>::max();//sampleRate * 3600 * 3; // 3hr for long duration test 
        break;
    default:
        result = false;
        break;
    }
    if (result == false) {
        cerr << "Set input source but invalid" << endl;
    }

    switch (param.outAudioType) {
    case SourceType::AudioFile:
        // manually assign audio format incase the output is file
        format = sther->GetFileFormat(param.outFileExt);
        if (format == 0) format = 65538;
        result = sther->SetOutputFile(param.outFilePath, sampleRate, 2, format);
        break;
    case SourceType::AudioDevice:
        result = sther->SetOutputStream(param.outDeviceIdx);
        break;
    default:
        result = false;
        break;
    }
    if (result == false) {
        cerr << "Set output source but invalid" << endl;
    }

    return result;
}

// stretcher times for begins and elasped
#ifdef _WIN32
RubberBand::
#endif
timeval tv;
#ifdef _WIN32
RubberBand::
#endif
timeval etv;

void processAudio(PitchShifting::Stretcher* sther, PitchShifting::Parameters* param) {
    sther->stop = false;
    sther->stopped = false;

    bool successful = false;
    int thisBlockSize;
    int defBlockSize = sther->GetDefBlockSize();
    double formantScale = param->formantscale;
    int64_t inputFrames = sther->totalFramesCount;

    while (!successful) { // we may have to repeat with a modified
        // gain, if clipping occurs
        successful = true;

        sther->Create();

        /* DEBUG: playground with channel data */
        if (param->gui) {
            mapDataPtrToGuiPlot(sther);
        }

        if (param->inAudioType == SourceType::AudioFile) {
            sther->ExpectedInputDuration(inputFrames); // estimate from input file
        }
        sther->MaxProcessSize(defBlockSize);
        sther->FormantScale(formantScale);
        sther->SetIgnoreClipping(param->ignoreClipping);

        // NOTE: study input sound here is now meaningless which, will not process twice with first run studying
        //if (!param->realtime) {
        //    sther->StudyInputSound(); // only works on input data source is file
        //}

        int frame = 0;
        int percent = 0;

        sther->SetKeyFrameMap();

        // reset counters
        sther->inputCount = 0;
        sther->outputCount = 0;
        bool clipping = false;

        // The stretcher only pads the start in offline mode; to avoid
        // a fade in at the start, we pad it manually in RT mode. Both
        // of these functions are defined to return zero in offline mode
        int toDrop = 0;
        toDrop = sther->ProcessStartPad();
        sther->SetDropFrames(toDrop);

        bool reading = true; // original offical sample is using isFinal, but reading is much fit to modified behavior
        while (reading) {

            thisBlockSize = defBlockSize;
            sther->ApplyFreqMap(sther->inputCount, &thisBlockSize);

            // frame number is actual given rubberband stretcher input frames, 
            // input count is read frames from input source
            bool isFinal = sther->ProcessInputSound(&frame, &sther->inputCount);

            // retrieve processed data to out buffer and result if clipping occurred as successful variable
            successful = sther->RetrieveAvailableData(&sther->outputCount, isFinal);

            if (frame == 0 && !param->realtime && !param->quiet) {
                cerr << "Pass 2: Processing..." << endl;
            }

            // show process percentage if input source is audio file(estimatable duration)
            if (param->inAudioType == SourceType::AudioFile) {
                int p = int((double(frame) * 100.0) / inputFrames);
                if (p > percent || frame == 0) {
                    percent = p;
                    if (!param->quiet) {
                        cerr << "\r" << percent << "% ";
                    }
                }
            }

            // exit while loop if all input frames are processed
            if (frame >= inputFrames && inputFrames > 0) {
                cerr << "=== End reading inputs f:" << frame << " c:" << sther->inputCount << " ===" << endl;
                // normally frame == inputCount if given audio file as input
                //if (sther->inSrcDesc.type == SourceType::AudioFile) {
                //    assert(frame, sther->inputCount);
                //}
                reading = false;
            }
            // peaceful leaving while loop if user press any key to cancel
            if (isWaitKeyPressed()) {
                cerr << "=== Cancel reading inputs ===" << endl;
                reading = false;
                successful = true;
            }
            // raised stop via GUI window state if destroyed to interrupt processing
            if (param->gui && sther->stop) {
                cerr << "=== GUI was destroyed to leave ===" << endl;
                reading = false;
            }
        } // while (reading)

        // NOTE: refer to stretcher::outGain, apply in realtime instead of restart from begin
        if (!successful) {
            cerr << "WARNING: found clipping during process, decrease gain and process from begin again.." << endl;
            cerr << "         but it is not concern to use audio device realtime applying adjustments" << endl;
            // original rubberband offical example design is reset file seek to 0 and restart whole process with lower output gain
            if (!param->realtime && sther->inSrcDesc.type == SourceType::AudioFile) {
                // reset file seek position and restart from begin
            }
            successful = true; // ignored, no further manipulation
        }

        if (!param->quiet) {
            cerr << "\r    " << endl;
        }

        // NOTE: get rest of availble processed blocks from stretcher
        //       based on current reading input design, this behavior may redundant
        sther->RetrieveAvailableData(&sther->outputCount);

    } // while (successful)
    sther->stopped = true;
}

int main(int argc, char **argv)
{
    PitchShifting::Parameters param;
    auto code = param.ParseOptions(argc, argv);
    if (code >= 0) return code;

    if (param.gui) {
        setGLWindow(&param);
        uiCreate(uiCallbackFnMap, &param);
        uiPrepareFrame(); // update a frame
        //DEBUG: following stretcher func may take time makes frame freeze except move sther into thread 
    }
    
    // start stretcher class initialization here
    const int defBlockSize = 1024;
    PitchShifting::Stretcher *sther = new PitchShifting::Stretcher(&param, defBlockSize, 1);

    sther->LoadTimeMap(param.timeMapFile);
    bool pitchToFreq = param.freqMapFile.empty();
    sther->LoadFreqMap((pitchToFreq ? param.pitchMapFile : param.freqMapFile), pitchToFreq);

    // so far use audio device frequently than files
    std::vector<SourceDesc> devices;
    int devCount = sther->ListAudioDevices(devices);
    // list audio device only
    if (param.listdev) {
        delete sther;
        return 0;
    }

    // only for gui combobox presents items
    std::vector<SourceDesc> files;
    int fileCount = sther->ListLocalFiles(files);
    cerr << "Local audio files: " << fileCount << endl;

    int sampleRate = 0;
    int channels = 0;
    int format = 0;
    int64_t inputFrames = 0;

    // check input/output audio file or device
    bool checkAudio = setAudioSource(param, sther, sampleRate, channels, format, inputFrames);

    if (param.gui) {
        // wait until all GUI classes constructed in another thread before calls GUI functions
        while (uiWindowState != FnWindowStates::INITIALIZED) {
            usleep(100000); // 100ms
        }

        // append list to control form, and defualt selection from cli options
        ctrlForm->SetAudioDeviceList(devices, param.inDeviceIdx, param.outDeviceIdx);
        ctrlForm->SetAudioFileList(files, 
            (param.inAudioType == SourceType::AudioFile ? param.inAudioParam : nullptr),
            (param.outAudioType == SourceType::AudioFile ? param.outAudioParam : nullptr));
        ctrlForm->SetAudioAdjustment(param.pitchshift, param.formantshift, param.inputgaindb);
    }
    
    if (checkAudio == false && param.gui == false) {
        // leave app if no available audio sources in console mode
        cerr << "Set input/output audio device failed" << endl;
        delete sther;
        return 1;
    }
    // in GUI mode, block process until user confirm the default input/output source selection
    if (param.gui) {
        checkAudio = false; // force to user confirm the selection
        // wait button event(from button clicked callback) after audio device selection in gui mode
        while (checkAudio == false) {
            //usleep(500000);
            // keep GUI frame rendering
            if (uiPrepareFrame() != 0) {
                // raise window state to DESTROYED
                uiTerminate(uiCallbackFnMap);
            }

            if (uiWindowState == FnWindowStates::DESTROYED) {
                cerr << "CLI given in/out source failed and GUI closed" << endl;
                delete sther;
                return 1;
            }

            bool audioSrcChanged = false;
            auto data = uiCtrlFormData.load();
            if (data == nullptr) continue;

            switch (uiSetAudioButton) {
            case GLUI::CtrlFormIds::SetInputDeviceButton:
                sther->CloseInputFile();
                param.inAudioType = SourceType::AudioDevice;
                param.inDeviceIdx = data->InputSource.index;
                audioSrcChanged = true;
                break;
            case GLUI::CtrlFormIds::SetOutputDeviceButton:
                sther->CloseOutputFile();
                param.outAudioType = SourceType::AudioDevice;
                param.outDeviceIdx = data->OutputSource.index;
                audioSrcChanged = true;
                break;
            case GLUI::CtrlFormIds::SetInputFileButton:
                sther->CloseInputStream();
                param.inAudioType = SourceType::AudioFile;
                param.inFilePath = data->InputSource.desc;
                audioSrcChanged = true;
                break;
            case GLUI::CtrlFormIds::SetAdjustmentButton:
                param.pitchshift = data->PitchShift;
                param.formantshift = data->FormantShift;
                param.formant = (param.formantshift != 0.0 ? true : false);
                param.inputgaindb = data->InputGain;
                cerr << "pitch/formant/gain changed by user " << param.pitchshift << ", " << param.formantshift << ", " << param.inputgaindb << endl;
                // just set, not go to check audio invoking start stretcher
                break;
            default:
                break;
            }
            //TODO: reset button id flag, at here? you sure?
            uiSetAudioButton = 0;

            if (audioSrcChanged) {
                checkAudio = setAudioSource(param, sther, sampleRate, channels, format, inputFrames);
            }
        }
    }
    // assign total frames count to stretcher after audio source accepted
    sther->totalFramesCount = inputFrames;

    // section for GUI initialization before stretcher creation(after ctor, but before rubberband configuration)
    if (param.gui) {
        // set audio information to GUI plot, given inFrame must afterward sther->SetInputStream for buffer initialization
        inWaveform->SetAudioInfo(sampleRate, channels, sther->inFrame, defBlockSize * channels);
        outWaveform->SetAudioInfo(
            sther->outSrcDesc.sampleRate,
            sther->outSrcDesc.outputChannels,
            sther->outFrame,
            defBlockSize * sther->outSrcDesc.outputChannels);
    }

    if (param.pitchshift != 0.0) {
        param.frequencyshift *= pow(2.0, param.pitchshift / 12.0);
        cerr << "Pitch shift semitones: " << param.pitchshift << endl;
    }

    cerr << "Using time ratio " << param.timeratio;

    if (!param.freqOrPitchMapSpecified) {
        cerr << " and frequency ratio " << param.frequencyshift << " for pitch shifting" << endl;
    } else {
        cerr << " and initial frequency ratio " << param.frequencyshift << "for pitch shifting" << endl;
    }

    // NOTE: formant adjustment only works to r3
    if (param.formantshift != 0.0) {
        cerr << "Formant shift semitones: " << param.formantshift << endl;
    }
    double formantShift = pow(2.0, param.formantshift / 12.0);

    // default formant scale = 1.0 / freq(pitch)shift if formant enabled
    if (param.formant) {
        // NOTE: pitch changes also affact formant scaling
        param.formantscale = 1.0 / param.frequencyshift;
        cerr << "Formant preserved default " << param.formantscale << " by pitch shift" << endl;
        param.formantscale *= formantShift;
        cerr << "Formant ratio " << formantShift << ", results formant scale " << param.formantscale << endl;
    }
    
    // apply gain, voltage level for audio signal will be pow(10.f, db / 20.f)
    if (param.inputgaindb != 0.0) {
        cerr << "Input gain db: " << param.inputgaindb << endl;
        double inputgainlv = pow(10.f, param.inputgaindb / 20.f);
        cerr << "Input gain level: " << inputgainlv << endl;
        sther->SetInputGain(inputgainlv);
    }

    (void)gettimeofday(&tv, 0);
    
    setWaitKey('q'); // set wait key for user interrupts process loop (mainly to cancel audio stream realtime works)

    sther->StartInputStream();
    sther->StartOutputStream();

    // run stretcher process in the thread isolate from main ui 
    auto bound = std::bind([](PitchShifting::Stretcher* stherPtr, PitchShifting::Parameters* paramPtr) {
        processAudio(stherPtr, paramPtr);
        }, sther, &param);
    stherThread = new std::thread(bound);
    stherThread->detach();

    // keep GUI rendering to display charts
    while (uiPrepareFrame() == 0) {
    }
    uiTerminate(uiCallbackFnMap);
    if (uiWindowState == FnWindowStates::DESTROYED) {
        sther->stop = true;
        while (sther->stopped == false) {
            usleep(10000);
        }
    }

    sther->CloseInputFile();
    sther->CloseOutputFile();

    //sther->WaitStream(); // DEBUG: no need to wait stream for callback, use main loop instead
    sther->StopInputStream();
    sther->StopOutputStream();

    if (!param.quiet) {
        auto countIn = sther->inputCount;
        auto countOut = sther->outputCount;
        cerr << "in: " << countIn << ", out: " << countOut
             << ", ratio: " << float(countOut)/float(countIn)
             << ", ideal output: " << lrint(countIn * param.timeratio)
             << ", error: " << int(countOut) - lrint(countIn * param.timeratio)
             << endl;


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


