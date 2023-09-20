#pragma once
/* the realtime time-domain plot chart (separated from Waveform class) */
#include "Waveform.h"

namespace GLUI {

class RealTimePlot : public PlotChartBase {
public:
	RealTimePlot(const char* posfix);

	/* given audio information from stretcher, assign to audioDevice and drawing buffer allocation
	 * NOTE: framePtr/ptrSize => frame buffer ptr and its size from port audio callback in stretcher class
	 */
	void SetAudioInfo(int samplerate, int channels, float* framePtr, int ptrSize);

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

	// for audio frame drawing on GUI
	AudioInfo audioDevice;
	// NOTE: decide to use the dirty way getting signal data from pa audio callback in stretcher class,
	//   'cause easiest way prevent blocking device callback or increasing effort on stretcher processing 
	float* framePtr = nullptr;
	// NOTE: in this project, audio frame size is fixed number by default FFT block size, refer to sther->defBlockSize
	//   the frame pointer size will be defBlockSize * channels
	int framePtrSize = 0;

	// for realtime plot
	bool realtimePlotEnabled;
	float currentTime;
	float elapsedRange;
	// maximum support 2 channels store plot points for realtime chart drawing
	AmplitudeBuffer realtimeBuffer[2];

}; // class

} // namespace GLUI