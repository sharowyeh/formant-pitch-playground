#include "helper.hpp"

using std::cerr;
using std::endl;

double tempo_convert(const char* str)
{
    char* d = strchr((char*)str, ':');

    if (!d || !*d) {
        double m = atof(str);
        if (m != 0.0) return 1.0 / m;
        else return 1.0;
    }

    char* a = strdup(str);
    char* b = strdup(d + 1);
    a[d - str] = '\0';
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

bool checkNumuric(char* arr, int* out) {
    // like apple isnumeric()
    try
    {
        auto val = std::stoi(arr);
        if (out) {
            *out = val;
        }
        return true;
    }
    catch (const std::exception&)
    {
    }
    return false;
}

std::atomic<bool> m_waitKeyPressed = false;
std::thread* m_waitKeyThread = nullptr;
bool isWaitKeyPressed() { return m_waitKeyPressed; }
void setWaitKey(int keyCode) {
    // ignore if thread is working
    if (m_waitKeyThread) {
        return;
    }
    m_waitKeyPressed = false;
    m_waitKeyThread = new std::thread([keyCode]() {
        while (true) {
            int key = getch();
            if (key == keyCode) {
                std::cout << "key " << keyCode << " pressed\n";
                break;
            }
        }
        m_waitKeyPressed = true;
        });
    // without blocking caller thread
    m_waitKeyThread->detach();
}
