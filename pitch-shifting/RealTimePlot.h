#pragma once
/* the realtime time-domain plot chart (separated from Waveform class) */
#include "Waveform.h"

namespace GLUI {

class RealTimePlot : public PlotChartBase {
public:
	RealTimePlot(const char* surffix);

	/* given audio information from stretcher, assign to audioDevice and drawing buffer allocation
	 * NOTE: framePtr/ptrSize => frame buffer ptr and its size from port audio callback in stretcher class
	 */
	void SetAudioInfo(int samplerate, int channels, float* framePtr, int ptrSize);

	void SetPitchInfo(double* pitchPtr, int ptrSize);

	// NOTE: all realtime plot instances will drawing on the same window(the same window id but identical widget id by surffix) 
	void Update() override;
	void UpdatePlot() override; // time-amp plot scrolling on short time window

protected:
	virtual ~RealTimePlot();
private:
	// for identical labels
	std::string realtimePlotEnableLabel;
	std::string realtimePlotRangeLabel;
	std::string realtimePlotTitle;
	// to chart combination with pitch plot, force waveform amplitude in positive value
	bool postiveOnly = true;

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

	// pitch plot for combination with waveform (likes koixxx app)
	double* pitchPtr = nullptr;
	int pitchPtrSize = 0;
	AmplitudeBuffer pitchBuffer;

}; // class

} // namespace GLUI