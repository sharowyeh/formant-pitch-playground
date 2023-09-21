#pragma once
/* DEBUG: i guess phase data can use line chart? */
#include "Waveform.h"

namespace GLUI {

class PhaseChart: public PlotChartBase {
public:
	PhaseChart(const char* surffix);
	
	/* NOTE: phase data is channel separated */
	void SetPhaseInfo(int ch, int fftsize, double* phases, int size);
	
	void Update();
	void UpdatePhasePlot();

protected:
	virtual ~PhaseChart() { };

private:
	std::string title;
	std::string phasePlotTitle;

	/* ptr from stretcher/rubber band channel data */
	int fftSize;
	int channels;
	double* phasesPtr;
	int bufSize;

	AmplitudeBuffer phaseBuffer[2];
};

} // namespace GLUI