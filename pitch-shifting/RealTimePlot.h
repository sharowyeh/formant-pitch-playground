#pragma once
/* the realtime time-domain plot chart (separated from Waveform class) */
#include "Waveform.h"

namespace GLUI {

class RealTimePlot : public PlotChartBase {
public:
	RealTimePlot(const char* posfix);

	/* given audio device info as rendering property */
	void SetDeviceInfo(int samplerate, int channels);
	/* given ring buffer from PA device callback in stretcher  */
	void SetFrameBuffer(RubberBand::RingBuffer<float>* buffer);

	void Update();
	void UpdateRealtimeWavPlot(); // time-amp plot scrolling on short time window

protected:
	virtual ~RealTimePlot();
private:
	// for identical labels
	std::string title;
	std::string realtimePlotEnableLabel;
	std::string realtimePlotRangeLabel;
	std::string realtimePlotTitle;

	AudioInfo audioDevice; // for SetInputFrames from stretcher ring buffer
	RubberBand::RingBuffer<float>* frameBuffer = nullptr; // frames ring buffer from stretcher

	// for realtime plot
	bool realtimePlotEnabled;
	float currentTime;
	float elapsedRange;
	// maximum support 2 channels store plot points for realtime chart drawing
	AmplitudeBuffer realtimeBuffer[2];

}; // class

} // namespace GLUI