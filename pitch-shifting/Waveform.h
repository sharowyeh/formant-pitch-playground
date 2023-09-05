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

constexpr auto PLOT_WIDTH_MAX = 4096;

/* signed amplitudes for plot chart, works like ring buffer */
typedef struct AmplitudeBuffer {
	int MaxSize; // ImVector uses int type to size
	int Offset;
	ImVector<Amplitude> Amplitudes;
	AmplitudeBuffer(int size = PLOT_WIDTH_MAX) {
		MaxSize = size;
		Offset = 0;
		Amplitudes.reserve(size);
	}

	void PushBack(float time, float positive, float negative) {
		if (Amplitudes.Size < MaxSize) {
			Amplitudes.push_back(Amplitude(time, positive, negative));
		}
		else {
			Amplitudes[Offset] = Amplitude(time, positive, negative);
			Offset = (Offset + 1) % MaxSize;
		}
	}
	// given default 0 all data will be erase
	void Resize(int size = 0) {
		if (size < MaxSize) {
			Amplitudes.shrink(size);
			if (Offset >= size) Offset = 0;
		} else if (size > MaxSize) {
			Amplitudes.reserve(size);
		}
		MaxSize = size;
	}
} _AMPLITUDE_BUFFER;

class Waveform {
public:
	Waveform();
	bool LoadAudioFile(std::string fileName, int* samplerate = nullptr, int* channels = nullptr, size_t* frames = nullptr, float** buf = nullptr);
	void ResampleAmplitudes(int width, double begin, double end, int samplerate, int frames, int channels, float* buf, int ch = 0);
	void Update();
	void UpdateWavPlot(); // time-amp plot
	void UpdateRealtimeWavPlot(); // time-amp plot scrolling on short time window
	void UpdateSptrPlot(); // time-freq-power plot
protected:
	virtual ~Waveform();
private:
	AudioInfo audio;
	// for zoom in/out on fill line plot
	float wavPlotWidth;
	double wavPlotBegin;
	double wavPlotEnd;
	ImVector<Amplitude> wavPlotBuffer[2];
	AmplitudeBuffer plotBuffer;
}; // class

} // namespace