#pragma once
/*
* Waveform presentation using ImPlot fill line plot and realtime plot chart
*/
#include "../imgui/implot.h"
#include "../imgui/implot_internal.h"

// for audio file access
#include <string>
#include <sndfile.h>

// for stretcher ringbuffer
#include <src/common/RingBuffer.h>

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

/* can just slightly larger than window/screen width */
constexpr auto PLOT_WIDTH_MAX = 2048;

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
	// given size 0 all data will be erase
	void Resize(int size = PLOT_WIDTH_MAX) {
		MaxSize = size;
		if (size < Amplitudes.capacity()) {
			Amplitudes.shrink(size);
		}
		else if (size > Amplitudes.capacity()) {
			Amplitudes.reserve(size);
		}
		if (Offset >= size) Offset = 0;
	}
	void Erase() {
		Resize(0);
	}
} _AMPLITUDE_BUFFER;

class Waveform {
public:
	Waveform();
	bool LoadAudioFile(std::string fileName, int* samplerate = nullptr, int* channels = nullptr, size_t* frames = nullptr, float** buf = nullptr);

	void SetInputAudioInfo(int samplerate, int channels);
	void SetInputFrame(RubberBand::RingBuffer<float>* frames);

	/* return min and max value from frame buffer from offset during the length */
	void GetRangeMinMax(int offset, int length, int frames, int channels, float* buf, int ch, float* maximum, float* minimum);
	void ResampleAmplitudes(int width, double begin, double end, int samplerate, int frames, int channels, float* buf, int ch = 0);
	void Update();
	void UpdateWavPlot(); // time-amp plot
	void UpdateRealtimeWavPlot(); // time-amp plot scrolling on short time window
	void UpdateSptrPlot(); // time-freq-power plot
protected:
	virtual ~Waveform();
private:
	AudioInfo audioFile; // for LoadAudioFile
	AudioInfo audioDevice; // for SetInputFrames from stretcher ring buffer
	RubberBand::RingBuffer<float>* inFrames = nullptr;

	bool wavPlotEnabled;
	// for wavform zoom in/out on fill line plot
	float wavPlotWidth;
	double wavPlotBegin;
	double wavPlotEnd;
	ImVector<Amplitude> wavPlotBuffer[2];
	// for realtime plot
	bool realtimePlotEnabled;
	float currentTime;
	float elapsedRange;
	// maximum support 2 channels
	AmplitudeBuffer realtimeBuffer[2];
}; // class

} // namespace