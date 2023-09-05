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
#include "Waveform.h"

GLUI::Waveform* waveform;

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

	waveform = new GLUI::Waveform();
	// DEBUG: read my debug audio
	waveform->LoadAudioFile("debug.wav");

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
		waveform->Update();

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