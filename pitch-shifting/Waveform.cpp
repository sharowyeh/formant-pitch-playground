#include "Waveform.h"

namespace GLUI {

Waveform::Waveform() {
	audio = { 0 };
	wavPlotWidth = 0;
	wavPlotBegin = 0;
	wavPlotEnd = 0;
	wavPlotBuffer[0].reserve(PLOT_WIDTH_MAX);

	realtimeEnabled = false;
	currentTime = 0;
	elapsedRange = 5.f;
}

Waveform::~Waveform() {
	if (audio.Buffer) {
		delete audio.Buffer;
		audio.Buffer = nullptr;
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
	audio.SampleRate = info->samplerate;
	audio.Channels = info->channels;
	audio.Frames = info->frames;
	printf("file %s:\n samplerate:%d, channels:%d, format:%d, frames:%lld\n", fileName.c_str(), info->samplerate, info->channels, info->format, info->frames);
	// return to caller if specific
	if (samplerate) *samplerate = info->samplerate;
	if (channels) *channels = info->channels;
	if (frames) *frames = info->frames;

	// rebuild data buffer
	if (audio.Buffer) {
		delete audio.Buffer;
	}
	// NOTE: it quite huge mem usage from a wave file...
	audio.Buffer = (float*)calloc(audio.Frames * audio.Channels, sizeof(float));
	float* tmp = audio.Buffer;
	// read all float audio data
	size_t read = 0;
	while (read < info->frames) {
		int count = sf_readf_float(f, tmp, 2048);
		tmp += count * info->channels;
		read += count;
	}
	printf("do something to buffer %p\n", audio.Buffer);
	// return to caller if pointer of buffer exists
	if (buf) *buf = audio.Buffer;

	// close file
	if (f) {
		sf_close(f);
	}

	return true;
}

void Waveform::MinMaxAmp(int offset, int step, int frames, int channels, float* buf, int ch, float* pos, float* neg) {
	*pos = 0.f;
	*neg = 0.f;
	if (offset >= frames || offset + step <= 0) {
		return;
	}
	// find min/max during step from buffer
	for (int i = 0; i < step; i++) {
		if (offset + i <= 0) continue;
		if (offset + i >= frames) break;
		// buffer payload just follow sndfile stride by channels
		size_t idx = (size_t)(offset + i) * channels + ch;
		if (*pos < buf[idx]) *pos = buf[idx];
		if (*neg > buf[idx]) *neg = buf[idx];
	}
}

void Waveform::ResampleAmplitudes(int width, double begin, double end, int samplerate, int frames, int channels, float* buf, int ch)
{
	int beginIndex = floor(begin * samplerate);
	int endIndex = floor(end * samplerate);
	double stride = (end - begin) / width;
	int step = floor(stride * samplerate);
	// loop by ui width pixel
	for (int i = 0; i < width; i++) {
		int idx = beginIndex + floor(stride * i * samplerate);
		float hi = -1.f;
		float lo = 1.f;
		// find max/min during step from buffer for each steps
		for (int j = 0; j < step; j++) {
			if (idx + j >= frames || idx + j < 0) {
				break;
			}
			// directly from sndfile buf with channels
			size_t pos = ((size_t)idx + j) * channels + ch;
			hi = (hi < buf[pos]) ? buf[pos] : hi;
			lo = (lo > buf[pos]) ? buf[pos] : lo;
		}
		// rescale time(x axis)
		if (wavPlotBuffer[ch].size() <= i) {
			wavPlotBuffer[ch].push_back(Amplitude(begin + stride * i, hi == -1.f ? 0 : hi, lo == 1.f ? 0 : lo));
		}
		else {
			wavPlotBuffer[ch][i] = Amplitude(begin + stride * i, hi == -1.f ? 0 : hi, lo == 1.f ? 0 : lo);
		}
	}
}

void Waveform::Update()
{
	ImGui::Begin("Waveform");
	UpdateWavPlot();
	//TODO: perf goes crazy cause plot chart resampling
	UpdateRealtimeWavPlot();
	ImGui::End();
}

void Waveform::UpdateWavPlot()
{
	// also reserve wav plot buffer for second audio channel(maximum only support 2 channels)
	if (audio.Channels > 1 && wavPlotBuffer[1].capacity() == 0)
		wavPlotBuffer[1].reserve(PLOT_WIDTH_MAX);

	if (ImPlot::BeginPlot("wav", ImVec2(-1, 300))) {
		ImPlot::SetupAxes("time", "amp");
		ImPlot::SetupAxisLimits(ImAxis_X1, 0, (audio.SampleRate) ? ((double)audio.Frames / audio.SampleRate) : 0);
		ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImPlotCond_Always); // fixed y-axis allows mouse control x-axis zoom, selection and scrolling
		auto width = ImPlot::GetPlotSize().x;
		auto begin = ImPlot::GetPlotLimits().X.Min;
		auto end = ImPlot::GetPlotLimits().X.Max;
		if (audio.Buffer && (wavPlotWidth != width || wavPlotBegin != begin || wavPlotEnd != end)) {
			for (auto ch = 0; ch < audio.Channels; ch++) {
				if (ch > 1) break;
				ResampleAmplitudes(width, begin, end, audio.SampleRate, audio.Frames, audio.Channels, audio.Buffer, ch);
			}
			wavPlotWidth = width;
			wavPlotBegin = begin;
			wavPlotEnd = end;
		}
		// fill out with amplitudes
		ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
		for (auto ch = 0; ch < audio.Channels; ch++) {
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
		float dx[2] = { 0.f, (audio.SampleRate) ? (float)audio.Frames / audio.SampleRate : 0.f };
		float dy[2] = { -1.f, 1.f };
		ImPlot::PlotLine("##dummy", dx, dy, 2);
		ImPlot::EndPlot();
	}
}

void Waveform::UpdateRealtimeWavPlot()
{
	// also reserve buffer for second audio channel(maximum only support 2 channels)
	if (audio.Channels > 1 && plotBuffer.Amplitudes[1].capacity() == 0) 
		plotBuffer.Resize(2, PLOT_WIDTH_MAX);

	//if (ImGui::Checkbox("Start", &realtimeEnabled)) {
		auto deltaTime = ImGui::GetIO().DeltaTime;
		// get amplitude min/max from buffer during current to current+delta 
		int beginIdx = floor(currentTime * audio.SampleRate);
		int step = floor(deltaTime * audio.SampleRate);
		currentTime += deltaTime;
		for (int ch = 0; ch < audio.Channels; ch++) {
			float positive = 0.f;
			float negative = 0.f;
			if (beginIdx < audio.Frames) {
				MinMaxAmp(beginIdx, step, audio.Frames, audio.Channels, audio.Buffer, ch, &positive, &negative);
			}
			plotBuffer.PushBack(currentTime, positive, negative, ch);
		}
	//}

	ImGui::SliderFloat("Range", &elapsedRange, 0.5f, 5.f, "%.1f s");
	ImGui::Text("size: %d offset: %d", plotBuffer.MaxSize, plotBuffer.Offset);
	
	if (ImPlot::BeginPlot("##wavrealtime", ImVec2(-1, 300))) {
		ImPlot::SetupAxes("amp", "time");
		ImPlot::SetupAxisLimits(ImAxis_X1, currentTime - elapsedRange, currentTime, ImGuiCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImGuiCond_Always);
		// NOTE: apply to all shaded, or use ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.25f) to specific
		ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
		for (auto ch = 0; ch < audio.Channels; ch++) {
			if (ch > 1) break;
			ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL);
			auto label = std::string("ch").append(std::to_string(ch));
			ImPlot::PlotShaded(label.c_str(),
				&plotBuffer.Amplitudes[ch][0].x,
				&plotBuffer.Amplitudes[ch][0].p,
				&plotBuffer.Amplitudes[ch][0].n,
				plotBuffer.Amplitudes[0].size(), 0, plotBuffer.Offset, 3 * sizeof(float));
			// equivalent to following 2 shaded filling with y_ref = 0
			/*ImPlot::PlotShaded(label.c_str(),
				&plotBuffer.Amplitudes[ch][0].x,
				&plotBuffer.Amplitudes[ch][0].p,
				plotBuffer.Amplitudes[0].size(), 0, 0, plotBuffer.Offset, 3 * sizeof(float));
			ImPlot::PlotShaded(label.c_str(),
				&plotBuffer.Amplitudes[ch][0].x,
				&plotBuffer.Amplitudes[ch][0].n,
				plotBuffer.Amplitudes[0].size(), 0, 0, plotBuffer.Offset, 3 * sizeof(float));*/
			ImPlot::PlotLine(label.c_str(),
				&plotBuffer.Amplitudes[ch][0].x,
				&plotBuffer.Amplitudes[ch][0].p,
				plotBuffer.Amplitudes[0].size(), 0, plotBuffer.Offset, 3 * sizeof(float));
			ImPlot::PlotLine(label.c_str(),
				&plotBuffer.Amplitudes[ch][0].x,
				&plotBuffer.Amplitudes[ch][0].n,
				plotBuffer.Amplitudes[0].size(), 0, plotBuffer.Offset, 3 * sizeof(float));
		}
		ImPlot::PopStyleVar();
		ImPlot::EndPlot();
	}
}

void Waveform::UpdateSptrPlot()
{
}

} // namespace