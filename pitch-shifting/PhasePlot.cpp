#include "PhasePlot.h"
// for PI, imgui_internal has IM_PI macro

GLUI::PhaseChart::PhaseChart(const char* surffix) : PlotChartBase(surffix)
{
	title = std::string("PhaseChart");
	phasePlotTitle = IdenticalLabel("phase");

	fftSize = 0;
	channels = 0;
	phasesPtr = nullptr;
	bufSize = 0; // basically is fftSize / 2 + 1, TODO: also bin count of fft, refer to GuidedPhaseAdvance class
}

void GLUI::PhaseChart::SetPhaseInfo(int ch, int fftsize, double* phases, int size)
{
	fftSize = fftsize;
	phasesPtr = phases;
	bufSize = size;

	if (channels < ch + 1) channels = ch + 1;
	// prepare phase plot data, TODO: no need ring buffer type
	if (ch < 2 && phaseBuffer[ch].Amplitudes.size() < size)
		phaseBuffer[ch].Amplitudes.reserve(size);
}

void GLUI::PhaseChart::Update()
{
	ImGui::Begin(title.c_str(), nullptr);
	
	UpdatePhasePlot();
	
	ImGui::End();
}

void GLUI::PhaseChart::UpdatePhasePlot()
{
	if (fftSize == 0 || bufSize == 0 || phasesPtr == nullptr) return;

	ImGui::Text("FFT size: %d, phase size: %d", fftSize, bufSize);

	for (int i = 0; i < bufSize; i++) {
		float pos = (phasesPtr[i] > 0 ? phasesPtr[i] : 0.f);
		float neg = (phasesPtr[i] < 0 ? phasesPtr[i] : 0.f);
		if (i < phaseBuffer[0].Amplitudes.size()) {
			phaseBuffer[0].Amplitudes[i].x = i;
			phaseBuffer[0].Amplitudes[i].p = pos;
			phaseBuffer[0].Amplitudes[i].n = neg;
		}
		else {
			phaseBuffer[0].PushBack(i, pos, neg);
		}
	}

	if (ImPlot::BeginPlot(phasePlotTitle.c_str(), ImVec2(-1, 300))) {
		ImPlot::SetupAxes("bin", "phase");
		ImPlot::SetupAxesLimits(0, bufSize, -IM_PI, IM_PI);
		ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
		ImPlot::PlotShaded("ch", 
			&phaseBuffer[0].Amplitudes[0].x,
			&phaseBuffer[0].Amplitudes[0].p,
			&phaseBuffer[0].Amplitudes[0].n,
			bufSize, 0, 0, 3 * sizeof(float));
		ImPlot::PopStyleVar();
		ImPlot::PlotLine("ch",
			&phaseBuffer[0].Amplitudes[0].x,
			&phaseBuffer[0].Amplitudes[0].p,
			bufSize, 0, 0, 3 * sizeof(float));
		ImPlot::PlotLine("ch",
			&phaseBuffer[0].Amplitudes[0].x,
			&phaseBuffer[0].Amplitudes[0].n,
			bufSize, 0, 0, 3 * sizeof(float));
		ImPlot::EndPlot();
	}
}
