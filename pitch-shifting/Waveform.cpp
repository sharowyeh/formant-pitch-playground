#include "Waveform.h"

#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#define sprintf sprintf_s
#endif

using std::string;

namespace GLUI {

Waveform::Waveform(const char* posfix) : PlotChartBase(posfix) {
	title = string("Waveform");
	wavPlotEnableLabel = IdenticalLabel("Enable Wavfile Plot");
	wavPlotTitle = IdenticalLabel("Wav");
	wavPlotFittingLabel = IdenticalLabel(nullptr, "dummy");

	audioFile = { 0 };

	wavPlotEnabled = false;
	wavPlotWidth = 0;
	wavPlotBegin = 0;
	wavPlotEnd = 0;
	wavPlotBuffer[0].reserve(PLOT_WIDTH_MAX);
}

Waveform::~Waveform() {
	if (audioFile.Buffer) {
		delete audioFile.Buffer;
		audioFile.Buffer = nullptr;
	}
}

bool Waveform::LoadAudioFile(std::string fileName, int* samplerate, int* channels, size_t* frames, float** buf) {
	// get audio file info via sndfile
	SNDFILE* f = nullptr;
	SF_INFO inf = { 0 };
	SF_INFO* info = &inf;
	f = sf_open(fileName.c_str(), SFM_READ, info);
	if (!f) {
		printf("error read file %s\n", fileName.c_str());
		return false;
	}
	audioFile.SampleRate = info->samplerate;
	audioFile.Channels = info->channels;
	audioFile.Frames = info->frames;
	printf("file %s:\n samplerate:%d, channels:%d, format:%d, frames:%lld\n", fileName.c_str(), info->samplerate, info->channels, info->format, info->frames);
	// return to caller if specific
	if (samplerate) *samplerate = info->samplerate;
	if (channels) *channels = info->channels;
	if (frames) *frames = info->frames;

	// rebuild data buffer
	if (audioFile.Buffer) {
		delete audioFile.Buffer;
	}
	// NOTE: it quite huge mem usage from a wave file...
	audioFile.Buffer = (float*)calloc(audioFile.Frames * audioFile.Channels, sizeof(float));
	float* tmp = audioFile.Buffer;
	// read all float audio data
	size_t read = 0;
	while (read < info->frames) {
		int count = sf_readf_float(f, tmp, 2048);
		tmp += count * info->channels;
		read += count;
	}
	printf("do something to buffer %p\n", audioFile.Buffer);
	// return to caller if pointer of buffer exists
	if (buf) *buf = audioFile.Buffer;

	// close file
	if (f) {
		sf_close(f);
	}

	return true;
}

void Waveform::ResampleAmplitudes(int width, double begin, double end, int samplerate, int frames, int channels, float* buf, int ch)
{
	int beginIndex = floor(begin * samplerate);
	int endIndex = floor(end * samplerate);
	double interval = (end - begin) / width;
	int sampleCnt = floor(interval * samplerate);
	// loop by ui width pixel
	for (int i = 0; i < width; i++) {
		int idx = beginIndex + floor(interval * samplerate * i);
		float high = -1.f;
		float low = 1.f;
		// get a min max represent an interval samples
		GetRangeMinMax(idx, sampleCnt, frames, channels, buf, ch, &high, &low);
		if (high == -1.f) high = 0.f;
		if (low == 1.f) low = 0.f;
		// rescale time(x axis)
		if (wavPlotBuffer[ch].size() <= i) {
			wavPlotBuffer[ch].push_back(Amplitude(begin + interval * i, high, low));
		}
		else {
			wavPlotBuffer[ch][i] = Amplitude(begin + interval * i, high, low);
		}
	}
}

void Waveform::Update()
{
	//TODO: should i give a default size?
	/* with consistent form title, all instances rendering together */
	ImGui::Begin(title.c_str(), nullptr);
	//TODO: perf goes crazy cause plot chart resampling
	UpdateWavPlot();
	ImGui::End();
}

void Waveform::UpdateWavPlot()
{
	if (ImGui::Checkbox(wavPlotEnableLabel.c_str(), &wavPlotEnabled)) {
	}

	if (wavPlotEnabled == false) return;

	// also reserve wav plot buffer for second audio channel(maximum only support 2 channels)
	if (audioFile.Channels > 1 && wavPlotBuffer[1].capacity() == 0)
		wavPlotBuffer[1].reserve(PLOT_WIDTH_MAX);
	
	if (ImPlot::BeginPlot(wavPlotTitle.c_str(), ImVec2(-1, 300))) {
		ImPlot::SetupAxes("time", "amp");
		ImPlot::SetupAxisLimits(ImAxis_X1, 0, (audioFile.SampleRate) ? ((double)audioFile.Frames / audioFile.SampleRate) : 0);
		ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImPlotCond_Always); // fixed y-axis allows mouse control x-axis zoom, selection and scrolling
		auto width = ImPlot::GetPlotSize().x;
		auto begin = ImPlot::GetPlotLimits().X.Min;
		auto end = ImPlot::GetPlotLimits().X.Max;
		if (audioFile.Buffer && (wavPlotWidth != width || wavPlotBegin != begin || wavPlotEnd != end)) {
			for (auto ch = 0; ch < audioFile.Channels; ch++) {
				if (ch > 1) break;
				ResampleAmplitudes(width, begin, end, audioFile.SampleRate, audioFile.Frames, audioFile.Channels, audioFile.Buffer, ch);
			}
			wavPlotWidth = width;
			wavPlotBegin = begin;
			wavPlotEnd = end;
		}
		// fill out with amplitudes
		ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
		for (auto ch = 0; ch < audioFile.Channels; ch++) {
			if (ch > 1) break;
			ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL);
			auto label = std::string("ch").append(std::to_string(ch));
			ImPlot::PlotShaded(label.c_str(),
				&wavPlotBuffer[ch][0].x,
				&wavPlotBuffer[ch][0].p,
				&wavPlotBuffer[ch][0].n,
				width, 0, 0/*always begins from plot chart t0*/, 3 * sizeof(float));
			ImPlot::PlotLine(label.c_str(),
				&wavPlotBuffer[ch][0].x,
				&wavPlotBuffer[ch][0].p,
				width, 0, 0, 3 * sizeof(float));
			ImPlot::PlotLine(label.c_str(),
				&wavPlotBuffer[ch][0].x,
				&wavPlotBuffer[ch][0].n,
				width, 0, 0, 3 * sizeof(float));
		}
		ImPlot::PopStyleVar();
		// dummy fitting line
		ImPlot::SetNextLineStyle(ImVec4(0, 0, 0, 0));
		float dx[2] = { 0.f, (audioFile.SampleRate) ? (float)audioFile.Frames / audioFile.SampleRate : 0.f };
		float dy[2] = { -1.f, 1.f };
		ImPlot::PlotLine(wavPlotFittingLabel.c_str(), dx, dy, 2);
		ImPlot::EndPlot();
	}
}

} // namespace