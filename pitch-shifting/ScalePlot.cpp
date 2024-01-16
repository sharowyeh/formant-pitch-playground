#include "ScalePlot.h"

GLUI::ScalePlot::ScalePlot(const char* surffix) : PlotChartBase(surffix),
	fftSize(0)
{
	title = std::string("Scales Data Plot");
	dataPlotTitle = IdenticalLabel("scale");
}

void GLUI::ScalePlot::SetPlotInfo(const char* name, int type, int ch, int fftsize, double* dataptr, int size)
{
	fftSize = fftsize;
	// NOTE: if type+ch 's dataptr or size will change in runtime, ScaleData can not be map key,
	// TODO: the dataptr was from rubberband library sharing fixed vector, it is not a ideal way using it directly
	// scale data identify by type and ch
	auto exist = plotBuffers.find(ScaleData(nullptr, type, ch));
	if (exist != plotBuffers.end()) {
		auto plot = &exist->second;
		if (plot->MaxSize < size)
			plot->Resize(size);
	}
	else {
		auto data = ScaleData(name, type, ch, dataptr, size);
		AmplitudeBuffer buffer(size);
		plotBuffers[data] = buffer;
	}
}

void GLUI::ScalePlot::SetPlotInfo(int type, int ch, int fftsize, double* dataptr, int size)
{
	auto label = std::string("type:")
		.append(std::to_string(type))
		.append(" ch:")
		.append(std::to_string(ch));

	SetPlotInfo(label.c_str(), type, ch, fftsize, dataptr, size);
}

void GLUI::ScalePlot::UpdatePlotWith(const ScaleData& data, AmplitudeBuffer& plotBuffer)
{
	auto bufSize = data.bufSize;
	auto dataPtr = data.dataPtr;

	if (bufSize == 0 || dataPtr == nullptr) return;

	for (int i = 0; i < bufSize; i++) {
		float pos = (dataPtr[i] > 0 ? dataPtr[i] : 0.f);
		float neg = (dataPtr[i] < 0 ? dataPtr[i] : 0.f);
		if (i < plotBuffer.Amplitudes.size()) {
			plotBuffer.Amplitudes[i].x = i;
			plotBuffer.Amplitudes[i].p = pos;
			plotBuffer.Amplitudes[i].n = neg;
		}
		else {
			plotBuffer.PushBack(i, pos, neg);
		}
	}

	auto label = data.label;
	ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
	ImPlot::PlotShaded(label.c_str(),
		&plotBuffer.Amplitudes[0].x,
		&plotBuffer.Amplitudes[0].p,
		&plotBuffer.Amplitudes[0].n,
		bufSize, 0, 0, 3 * sizeof(float));
	ImPlot::PopStyleVar();
	ImPlot::PlotLine(label.c_str(),
		&plotBuffer.Amplitudes[0].x,
		&plotBuffer.Amplitudes[0].p,
		bufSize, 0, 0, 3 * sizeof(float));
	ImPlot::PlotLine(label.c_str(),
		&plotBuffer.Amplitudes[0].x,
		&plotBuffer.Amplitudes[0].n,
		bufSize, 0, 0, 3 * sizeof(float));
}

void GLUI::ScalePlot::UpdatePlot()
{
	if (fftSize == 0 || plotBuffers.size() == 0) return;

	auto bufSize = plotBuffers.begin()->first.bufSize;

	//TODO: buf size usually is 513(1024/2+1) or 1025(2048/2+1) also lower than GUI px size, could be down resampling to increase perf
	//TODO2: bin count(b1-b0) for magnitude/polar spectural usually is fftsize/2+1  
	ImGui::Text("FFT size: %d, phase size: %d, bin count:%d", fftSize, bufSize, fftSize / 2 + 1);

	if (ImPlot::BeginPlot(dataPlotTitle.c_str(), ImVec2(-1, 300))) {
		ImPlot::SetupAxes("bin", "phase");
		// increase x range for labels, real/imaginary amplitubes may need log op as db
		ImPlot::SetupAxesLimits(0 - bufSize / 4, bufSize / 8 * 9, -IM_PI, IM_PI);
		
		for (auto it = plotBuffers.begin(); it != plotBuffers.end(); it++) {
			UpdatePlotWith(it->first, it->second);
		}
		
		ImPlot::EndPlot();
	}
}
