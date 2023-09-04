#include "wmain.hpp"

#include <stdio.h>

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_glfw.h"
#include "../imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "../imgui/implot.h"
#include "../imgui/implot_internal.h"

#ifdef _DEBUG
#pragma comment(lib, "glfw3dll.lib")
#pragma comment(lib, "glfw3.lib")
#else
#pragma comment(lib, "glfw3_mt.lib")
#endif
#pragma comment(lib, "opengl32.lib")

#include "ExitPopup.h"

#include <string>
#include <sndfile.h>

float* ibuf = nullptr;
SF_INFO iinfo = { 0 };

// refer to stretcher->LoadInputFile
void getAudioData(std::string fileName, int* samplerate = nullptr, int* channels = nullptr, int* frames = nullptr) {
	// get audio file info via sndfile
	SNDFILE *f = nullptr;
	//SF_INFO i = { 0 };
	SF_INFO* info = &iinfo;
	f = sf_open(fileName.c_str(), SFM_READ, info);
	if (!f) {
		printf("error read file %s\n", fileName.c_str());
		return;
	}
	printf("file %s:\n samplerate:%d, channels:%d, format:%d, frames:%d\n", fileName.c_str(), info->samplerate, info->channels, info->format, info->frames);
	
	if (ibuf) {
		delete ibuf;
	}
	ibuf = (float*)calloc((size_t)info->frames * info->channels, sizeof(float));
	float* tmp = ibuf;
	// read all float audio data
	size_t read = 0;
	while (read < info->frames) {
		int count = sf_readf_float(f, tmp, 2048);
		tmp += count * info->channels;
		read += count;
	}
	printf("do something to ibuf %p\n", ibuf);

	//delete ibuf;

	// close file
	if (f) {
		sf_close(f);
	}
}

#define WAVEFORM_WIDTH_MAX 16384

// ch0
float x1TimePts[WAVEFORM_WIDTH_MAX] = { 0 };
float y1HiPts[WAVEFORM_WIDTH_MAX] = { 0 };
float y1LoPts[WAVEFORM_WIDTH_MAX] = { 0 };
// ch1
float x2TimePts[WAVEFORM_WIDTH_MAX] = { 0 };
float y2HiPts[WAVEFORM_WIDTH_MAX] = { 0 };
float y2LoPts[WAVEFORM_WIDTH_MAX] = { 0 };
float viewWidth = 0.f;
double viewBegin = 0;
double viewEnd = 0;

// resampling for UI, otherwise poor performance
void resample(int width, double begin, double end, int samplerate, int frames, int channels, float* buf, int ch = 0) {
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
		if (ch == 0) {
			x1TimePts[i] = begin + stride * i;
			y1HiPts[i] = hi == -1.f ? 0 : hi;
			y1LoPts[i] = lo == 1.f ? 0 : lo;
		}
		else {
			x2TimePts[i] = begin + stride * i;
			y2HiPts[i] = hi == -1.f ? 0 : hi;
			y2LoPts[i] = lo == 1.f ? 0 : lo;
		}
	}

}

void drawLinePlots() {
	// x data depends on frames of time from sample rate, 
	// eg., plot chart acquites 1s length of 44.1k, each x will be increased by 1/44.1k and total x data will have 44100 data points
	// y data need to normalized by -1 to 1
	// NOTE: resample to GUI for better performance
	ImGui::Begin("Waveform");
	if (ImPlot::BeginPlot("wav", ImVec2(-1, -1))) {
		ImPlot::SetupAxes("time", "amp");
		ImPlot::SetupAxisLimits(ImAxis_X1, 0, (iinfo.samplerate) ? ((double)iinfo.frames / iinfo.samplerate) : 0);
		ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImPlotCond_Always);
		auto width = ImPlot::GetPlotSize().x;
		auto begin = ImPlot::GetPlotLimits().X.Min;
		auto end = ImPlot::GetPlotLimits().X.Max;
		if (ibuf && (viewWidth != width || viewBegin != begin || viewEnd != end)) {
			resample(width, begin, end, iinfo.samplerate, iinfo.frames, iinfo.channels, ibuf);
			if (iinfo.channels > 1) {
				resample(width, begin, end, iinfo.samplerate, iinfo.frames, iinfo.channels, ibuf, 1);
			}
			viewWidth = width;
			viewBegin = begin;
			viewEnd = end;
		}
		// fill out by hiline, loline
		ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
		ImPlot::PlotShaded("ch0", x1TimePts, y1LoPts, y1HiPts, width);
		ImPlot::PlotLine("ch0", x1TimePts, y1LoPts, width);
		ImPlot::PlotLine("ch0", x1TimePts, y1HiPts, width);
		if (iinfo.channels > 1) {
			ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL);
			ImPlot::PlotShaded("ch1", x2TimePts, y2LoPts, y2HiPts, width);
			ImPlot::PlotLine("ch1", x2TimePts, y2LoPts, width);
			ImPlot::PlotLine("ch1", x2TimePts, y2HiPts, width);
		}
		ImPlot::PopStyleVar();
		// dummy fitting line
		ImPlot::SetNextLineStyle(ImVec4(0, 0, 0, 0));
		float dx[2] = { 0.f, (iinfo.samplerate) ? (float)iinfo.frames / iinfo.samplerate : 0.f };
		float dy[2] = { -1.f, 1.f };
		ImPlot::PlotLine("##dummy", dx, dy, 2);
		ImPlot::EndPlot();
	}
	ImGui::End();
}

int glfwWindowMain() {
	GLFWwindow* window;

	glfwSetErrorCallback([](int code, const char* msg) {
		printf("error:%d msg:%s\n", code, msg);
		});

	/* init framework */
	if (!glfwInit())
		return -1;

	/* remove window caption */
	//glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
	/* topmost */
	//glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

	window = glfwCreateWindow(1280, 720, "GLFW", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return -1;
	}

	glfwSetWindowOpacity(window, 0.8f);
	auto close_callback = [](GLFWwindow* wnd) {
		// this callback not design for rendering loops, all things can do is releasing resources before window closing
		printf("on windoe close callback\n");
	};
	glfwSetWindowCloseCallback(window, close_callback);

	/* make context current */
	glfwMakeContextCurrent(window);
	/* for vsync */
	glfwSwapInterval(1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	/* CreateContext -> Initialize -> default ini handlers */
	// apply custom callbacks to settings handler

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	// just like my vs2022
	io.Fonts->AddFontFromFileTTF("CascadiaMono.ttf", 20.f);
	io.FontDefault = io.Fonts->Fonts.back();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	// assign for exiting behavior
	GLUI::exit_popup_Init(window, (GLFWwindowclosefun)close_callback);

	// DEBUG: read my debug audio
	getAudioData("debug.wav");

	/* loop until user close window */
	while (glfwWindowShouldClose(window) == 0) {

		/* poll for and process event */
		glfwPollEvents();
		/* render after clear */
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClear(GL_COLOR_BUFFER_BIT);

		// TODO: mac error can't get backend data from opengl3
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.f); // must pair with PopStyleVar() restore style changes for rendering loop

		//ImGui::ShowDemoWindow();
		//bool open = true;
		//ImPlot::ShowDemoWindow(&open);
		drawLinePlots();

		if (glfwWindowShouldClose(window)) {
			GLUI::exit_popup_Update(true);
			// reset main window close flag, let main window close popup decide
			glfwSetWindowShouldClose(window, GLFW_FALSE);
		}
		GLUI::exit_popup_Render();

		ImGui::PopStyleVar();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		/* swap buffer */
		glfwSwapBuffers(window);
	}

	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();

	return 0;
}