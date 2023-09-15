#include "RealTimePlot.h"

using std::string;

GLUI::RealTimePlot::RealTimePlot(const char* posfix) : PlotChartBase(posfix) {
	title = string("Realtime Waveform");

	realtimePlotEnableLabel = IdenticalLabel("Enable Realtime Plot");
	realtimePlotTitle = IdenticalLabel(nullptr, "realtime");
	realtimePlotRangeLabel = IdenticalLabel("Range");

	audioDevice = { 0 };
	frameBuffer = nullptr;

	realtimePlotEnabled = false;
	currentTime = 0;
	elapsedRange = 5.f;
}

GLUI::RealTimePlot::~RealTimePlot() {
	if (audioDevice.Buffer) {
		delete audioDevice.Buffer;
		audioDevice.Buffer = nullptr;
	}
}

void GLUI::RealTimePlot::SetDeviceInfo(int samplerate, int channels) {
	audioDevice.SampleRate = samplerate;
	audioDevice.Channels = channels;
}

void GLUI::RealTimePlot::SetFrameBuffer(RubberBand::RingBuffer<float>* buffer) {
	frameBuffer = buffer;

	// audio info must have larger than 1 sec as temperary buffer for resampling
	audioDevice.Frames = audioDevice.SampleRate;
	if (audioDevice.Buffer) {
		delete audioDevice.Buffer;
		audioDevice.Buffer = nullptr;
	}
	audioDevice.Buffer = (float*)calloc(audioDevice.Frames * audioDevice.Channels, sizeof(float));
}

void GLUI::RealTimePlot::Update() {
	//TODO: should i give a default size?
	/* with consistent form title, all instances rendering together */
	ImGui::Begin(title.c_str(), nullptr, ImGuiWindowFlags_None);
	//TODO: perf goes crazy cause plot chart resampling
	UpdateRealtimeWavPlot();
	ImGui::End();
}

void GLUI::RealTimePlot::UpdateRealtimeWavPlot() {
	if (ImGui::Checkbox(realtimePlotEnableLabel.c_str(), &realtimePlotEnabled) == false) {
	}

	if (realtimePlotEnabled == false) return;

	// makes sure input device has been initialized (by SetInputAudioInfo and SetInputFrame
	if (audioDevice.SampleRate == 0 || audioDevice.Channels == 0 || frameBuffer == nullptr) return;

	ImGui::SameLine();
	ImGui::SliderFloat(realtimePlotRangeLabel.c_str(), &elapsedRange, 0.5f, 5.f, "%.1f s", ImGuiSliderFlags_None);

	// realtimeBuffer[n] has owned struct ctor with default size, just use it as well

	auto deltaTime = ImGui::GetIO().DeltaTime;
	// get amplitude min/max from buffer during current to current+delta 
	int beginIdx = floor(currentTime * audioDevice.SampleRate); // no use to audio device stream
	int sampleCnt = floor(deltaTime * audioDevice.SampleRate);
	currentTime += deltaTime;
	// NOTE: for variable size of audio stream device via ring buffer, may only focus sampleCnt and readable size
	//   retrieve ring buffer to our continuous temperary buffer for plot chart resampling
	//   the `readable` number is samples * channels
	// NOTE: based on frame size of port audio callback was set by Stretcher::defaultBlockSize (for FFT),
	//   each frame slices are always block size(1024) * channels(2) = 2048
	//   the GUI refresh rate can very, depends on performance that may not regularly consume audio frame from the buffer
	int readable = frameBuffer->getReadSpace();
	while (readable > sampleCnt * audioDevice.Channels) {
		// drop frames, TODO: or keep double buffer?
		frameBuffer->read(audioDevice.Buffer, readable - sampleCnt * audioDevice.Channels);
		std::cout << "drop GUI frames readable:" << readable << " required sample cnt:" << sampleCnt << " chs:" << audioDevice.Channels << std::endl;
		readable = frameBuffer->getReadSpace();
	}
	if (readable < sampleCnt * audioDevice.Channels) {
		//TODO: plot is works but seems not stable in reading
		std::cout << "readable:" << readable << " is less than required sample cnt:" << sampleCnt << " chs:" << audioDevice.Channels << std::endl;
	}
	frameBuffer->read(audioDevice.Buffer, readable);
	int frames = readable / audioDevice.Channels;
	for (int ch = 0; ch < audioDevice.Channels; ch++) {
		if (ch > 1) break;
		float positive = 0.f;
		float negative = 0.f;

		if (frames > 0) {
			GetRangeMinMax(0, sampleCnt, frames, audioDevice.Channels, audioDevice.Buffer, ch, &positive, &negative);
		}
		realtimeBuffer[ch].PushBack(currentTime, positive, negative);
	}

	if (ImPlot::BeginPlot(realtimePlotTitle.c_str(), ImVec2(-1, 300))) {
		ImPlot::SetupAxes("amp", "time");
		ImPlot::SetupAxisLimits(ImAxis_X1, currentTime - elapsedRange, currentTime, ImGuiCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImGuiCond_Always);
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
		ImPlot::PopStyleVar();
		ImPlot::EndPlot();
	}
}
