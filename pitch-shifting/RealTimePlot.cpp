#include "RealTimePlot.h"

using std::string;

GLUI::RealTimePlot::RealTimePlot(const char* surffix) : PlotChartBase(surffix) {
	title = string("Realtime Waveform");

	realtimePlotEnableLabel = IdenticalLabel("Enable Realtime Plot");
	realtimePlotTitle = IdenticalLabel(nullptr, "realtime");
	realtimePlotRangeLabel = IdenticalLabel("Range");

	postiveOnly = true;

	audioDevice = { 0 };
	framePtr = nullptr;
	framePtrSize = 0;

	realtimePlotEnabled = true;
	currentTime = 0;
	elapsedRange = 5.f;

	pitchPtr = nullptr;
	pitchPtrSize = 0;
}

GLUI::RealTimePlot::~RealTimePlot() {
	if (audioDevice.Buffer) {
		delete audioDevice.Buffer;
		audioDevice.Buffer = nullptr;
	}
}

void GLUI::RealTimePlot::SetAudioInfo(int samplerate, int channels, float* ptr, int size) {
	audioDevice.SampleRate = samplerate;
	audioDevice.Channels = channels;
	framePtr = ptr;
	framePtrSize = size;
	// based on given parameters for audioDevice buffer allocation
	audioDevice.Frames = size;
	if (audioDevice.Buffer) {
		delete audioDevice.Buffer;
		audioDevice.Buffer = nullptr;
	}
	audioDevice.Buffer = (float*)calloc(audioDevice.Frames * audioDevice.Channels, sizeof(float));
}

void GLUI::RealTimePlot::SetPitchInfo(double* ptr, int size) {
	pitchPtr = ptr;
	pitchPtrSize = size;

}

void GLUI::RealTimePlot::Update() {
	/* override PlotChartBase::Updates */
	
	ImGui::Begin(title.c_str(), nullptr, ImGuiWindowFlags_None);
	// NOTE: aware performance issue during drawing plot points just like
	UpdatePlot();
	ImGui::End();
}

void GLUI::RealTimePlot::UpdatePlot() {
	if (ImGui::Checkbox(realtimePlotEnableLabel.c_str(), &realtimePlotEnabled) == false) {
	}

	if (realtimePlotEnabled == false) return;

	// makes sure input device has been initialized (by SetInputAudioInfo and SetInputFrame
	if (audioDevice.SampleRate == 0 || audioDevice.Channels == 0 || framePtrSize == 0 || framePtr == nullptr) return;

	ImGui::SameLine();
	ImGui::SliderFloat(realtimePlotRangeLabel.c_str(), &elapsedRange, 0.5f, 5.f, "%.1f s", ImGuiSliderFlags_None);

	// realtimeBuffer[n] has owned struct ctor with default size, just use it as well

	auto deltaTime = ImGui::GetIO().DeltaTime;
	// get amplitude min/max from buffer during current to current+delta 
	int beginIdx = floor(currentTime * audioDevice.SampleRate); // no use to audio device stream
	int sampleCnt = floor(deltaTime * audioDevice.SampleRate);
	currentTime += deltaTime;

	// NOTE: based on frame size of port audio callback was set by Stretcher::defaultBlockSize (for FFT),
	//   each frame slices are always block size(1024) * channels(2) = 2048
	//   the GUI refresh rate can very, depends on performance that may not regularly consume audio frame from the buffer

	// NOTE: just make drawing simply, only use minimal availble samples whether GUI refresh rate changes 
	int readable = framePtrSize;
	if (readable > sampleCnt * audioDevice.Channels) {
		//std::cout << "required sample:" << sampleCnt << " chs:" << audioDevice.Channels << " readable:" << readable << std::endl;
		readable = sampleCnt * audioDevice.Channels;
	}
	if (readable < sampleCnt * audioDevice.Channels) {
		//std::cout << "readable:" << readable << " is less than required sample cnt:" << sampleCnt << " chs:" << audioDevice.Channels << std::endl;
	}
	//TODO: since change to use single frame buffer, it has poor sampling graph(may cause by memory leak...)
	//TODO: try to use stretcher buffer directly...
	//memcpy_s(audioDevice.Buffer, readable, framePtr, readable);
	int frames = readable / audioDevice.Channels;
	for (int ch = 0; ch < audioDevice.Channels; ch++) {
		if (ch > 1) break;
		float positive = 0.f;
		float negative = 0.f;

		if (frames > 0) {
			if (postiveOnly == false) {
				GetRangeMinMax(0, sampleCnt, frames, audioDevice.Channels, framePtr/*audioDevice.Buffer*/, ch, &positive, &negative);
			}
			else {
				GetPositiveMax(0, sampleCnt, frames, audioDevice.Channels, framePtr, ch, &positive);
			}
		}
		realtimeBuffer[ch].PushBack(currentTime, positive, negative);
	}

	// fill pitch data if assigned
	if (pitchPtr && pitchPtrSize > 0) {
		// normalize all fftSize between 0 to 1.f, find which bin has maximum value for the plot
		float bin = 0.f;
		float power = 0.f;
		// TODO: we need averagely large power of a seminote range instead of which power of bin is largest
		bin /= pitchPtrSize;
		pitchBuffer.PushBack(currentTime, bin, power);
	}

	if (ImPlot::BeginPlot(realtimePlotTitle.c_str(), ImVec2(-1, 300))) {
		ImPlot::SetupAxes("amp", "time");
		ImPlot::SetupAxisLimits(ImAxis_X1, currentTime - elapsedRange, currentTime, ImGuiCond_Always);
		if (postiveOnly == false) {
			ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImGuiCond_Always);
		}
		else {
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1, ImGuiCond_Always);
		}
		// NOTE: apply to all shaded, or use ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.25f) to specific
		ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
		for (auto ch = 0; ch < audioDevice.Channels; ch++) {
			if (ch > 1) break;
			ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL);
			auto label = std::string("ch").append(std::to_string(ch));
			ImPlot::PlotShaded(label.c_str(),
				&realtimeBuffer[ch].Amplitudes[0].x,
				&realtimeBuffer[ch].Amplitudes[0].p,
				&realtimeBuffer[ch].Amplitudes[0].n,
				realtimeBuffer[ch].Amplitudes.size(), 0, realtimeBuffer[ch].Offset, 3 * sizeof(float));
			// equivalent to following 2 shaded filling with y_ref = 0
			/*ImPlot::PlotShaded(label.c_str(),
				&realtimeBuffer[ch].Amplitudes[0].x,
				&realtimeBuffer[ch].Amplitudes[0].p,
				realtimeBuffer[ch].Amplitudes.size(), 0, 0, realtimeBuffer[ch].Offset, 3 * sizeof(float));*/
			/*ImPlot::PlotShaded(label.c_str(),
					&realtimeBuffer[ch].Amplitudes[0].x,
					&realtimeBuffer[ch].Amplitudes[0].n,
					realtimeBuffer[ch].Amplitudes.size(), 0, 0, realtimeBuffer[ch].Offset, 3 * sizeof(float));*/
			ImPlot::PlotLine(label.c_str(),
				&realtimeBuffer[ch].Amplitudes[0].x,
				&realtimeBuffer[ch].Amplitudes[0].p,
				realtimeBuffer[ch].Amplitudes.size(), 0, realtimeBuffer[ch].Offset, 3 * sizeof(float));
			ImPlot::PlotLine(label.c_str(),
				&realtimeBuffer[ch].Amplitudes[0].x,
				&realtimeBuffer[ch].Amplitudes[0].n,
				realtimeBuffer[ch].Amplitudes.size(), 0, realtimeBuffer[ch].Offset, 3 * sizeof(float));
		}
		// draw pitch
		if (pitchPtr && pitchPtrSize > 0) {
			ImPlot::PlotLine("pitch", &pitchBuffer.Amplitudes[0].x, &pitchBuffer.Amplitudes[0].p,
				pitchBuffer.Amplitudes.size(), 0, pitchBuffer.Offset, 3 * sizeof(float));
			ImPlot::PlotLine("power(pitch)", &pitchBuffer.Amplitudes[0].x, &pitchBuffer.Amplitudes[0].n,
				pitchBuffer.Amplitudes.size(), 0, pitchBuffer.Offset, 3 * sizeof(float));
		}
		ImPlot::PopStyleVar();
		ImPlot::EndPlot();
	}
}
