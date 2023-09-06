#pragma once
/*
* Waveform presentation using ImPlot fill line plot and realtime plot chart
*/
#include "../imgui/implot.h"
#include "../imgui/implot_internal.h"

// for audio file access
#include <string>
#include <sndfile.h>

namespace GLUI {

typedef struct AudioInfo {
	int SampleRate = 0;
	int Channels = 0;
	// double duration can be frames/samplerate
	size_t Frames = 0;
	float* Buffer = nullptr;
} _AUDIO_INFO;

struct Amplitude {
	float x, p, n;
	constexpr Amplitude() :x(0.f), p(0.f), n(0.f) {}
	constexpr Amplitude(float _x, float _p, float _n) : x(_x), p(_p), n(_n) {}
};

constexpr auto PLOT_WIDTH_MAX = 3072;

/* signed amplitudes for plot chart, works like ring buffer */
typedef struct AmplitudeBuffer {
	int MaxSize; // ImVector uses int type to size
	int Offset;
	int Channels;
	ImVector<Amplitude> Amplitudes[2];
	AmplitudeBuffer(int chs = 1, int size = PLOT_WIDTH_MAX) {
		MaxSize = size;
		Offset = 0;
		Channels = chs;
		for (int ch = 0; ch < chs; ch++) {
			if (ch > 1) break;
			Amplitudes[ch].reserve(size);
		}
	}

	void PushBack(float time, float positive, float negative, int ch = 0) {
		if (Amplitudes[ch].Size < MaxSize) {
			Amplitudes[ch].push_back(Amplitude(time, positive, negative));
		}
		else {
			Amplitudes[ch][Offset] = Amplitude(time, positive, negative);
			Offset = (Offset + 1) % MaxSize;
		}
	}
	// given default 0 all data will be erase
	void Resize(int chs = 1, int size = 0) {
		MaxSize = size;
		Channels = chs;
		for (int ch = 0; ch < chs; ch++) {
			if (ch > 1) break;
			if (size < Amplitudes[ch].capacity()) {
				Amplitudes[ch].shrink(size);
			}
			else if (size > Amplitudes[ch].capacity()) {
				Amplitudes[ch].reserve(size);
			}
		}
		if (Offset >= size) Offset = 0;
	}
	void Erase() {
		for (int ch = 0; ch < Channels; ch++) {
			if (Amplitudes[ch].size() > 0) {
				Amplitudes[ch].shrink(0);
			}
		}
		Offset = 0;
	}
} _AMPLITUDE_BUFFER;

class Waveform {
public:
	Waveform();
	bool LoadAudioFile(std::string fileName, int* samplerate = nullptr, int* channels = nullptr, size_t* frames = nullptr, float** buf = nullptr);
	void MinMaxAmp(int offset, int step, int frames, int channels, float* buf, int ch, float* pos, float* neg);
	void ResampleAmplitudes(int width, double begin, double end, int samplerate, int frames, int channels, float* buf, int ch = 0);
	void Update();
	void UpdateWavPlot(); // time-amp plot
	void UpdateRealtimeWavPlot(); // time-amp plot scrolling on short time window
	void UpdateSptrPlot(); // time-freq-power plot
protected:
	virtual ~Waveform();
private:
	AudioInfo audio;
	// for wavform zoom in/out on fill line plot
	float wavPlotWidth;
	double wavPlotBegin;
	double wavPlotEnd;
	ImVector<Amplitude> wavPlotBuffer[2];
	// for realtime plot
	bool realtimeEnabled;
	float currentTime;
	float elapsedRange;
	AmplitudeBuffer plotBuffer;
}; // class

} // namespace