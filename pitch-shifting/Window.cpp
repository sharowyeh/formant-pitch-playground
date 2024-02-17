/* NOTE: the original Windows class source code was duplicated from Iolive project */

#include "Window.hpp"
#include <stdexcept>
#include <thread>
#include <chrono>
#include <iostream>

// for ImGui initialization
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_glfw.h"
#include "../imgui/imgui_impl_opengl3.h"
#include "../imgui/implot.h"
#include "../imgui/implot_internal.h"

namespace GLUI {
	Window* Window::Create(const char* title, int width, int height)
	{
		if (!s_Window)
			s_Window = new Window(title, width, height);

		return s_Window;
	}

	Window* Window::Get()
	{
		std::lock_guard<std::mutex> lock(s_MtxGetInstance);
		return s_Window;
	}

	Window::Window(const char* title, int width, int height)
	{
		if (!glfwInit()) throw std::runtime_error("Can't initialize GLFW!");

#ifdef _WIN32
		/* in my windows dev env default */
		const char* glsl_version = "#version 330";
#elif defined(__APPLE__)
		// GL 3.2 + GLSL 150
		const char* glsl_version = "#version 150";
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#endif
		// window style can only after glfwinit before create window
		/* remove window caption */
		//glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
		/* topmost */
		//glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

		m_GlfwWindow = glfwCreateWindow(width, height, title, NULL, NULL);
		if (!m_GlfwWindow) throw std::runtime_error("Can't create window!");

		glfwSetWindowUserPointer(m_GlfwWindow, this);

		// create window context first
		glfwMakeContextCurrent(m_GlfwWindow);
		
		/* for vsync, fixed maximum 60 FPS in windowed mode */
		//glfwSwapInterval(1);

		// then initialize glew for further support texture 2d features
		//if (glewInit() != GLEW_OK)
		//	throw std::runtime_error("Can't initialize opengl loader");

		/*
		* Set OpenGL window callback to our delegate callback function
		*/
		glfwSetWindowCloseCallback(m_GlfwWindow, [](GLFWwindow* window) {
			Window* thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (!(thisWindow->OnWindowClosing)) return;

			thisWindow->OnWindowClosing(thisWindow);
			});

		glfwSetFramebufferSizeCallback(m_GlfwWindow, [](GLFWwindow* window, int width, int height) {
			Window* thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (!(thisWindow->OnFrameResizedCallback)) return;

			thisWindow->OnFrameResizedCallback(width, height);
			});

		glfwSetScrollCallback(m_GlfwWindow, [](GLFWwindow* window, double xoffset, double yoffset) {
			Window* thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (!(thisWindow->OnScrollCallback)) return;

			thisWindow->OnScrollCallback(xoffset, yoffset);
			});

		glfwSetCursorPosCallback(m_GlfwWindow, [](GLFWwindow* window, double xpos, double ypos) {
			Window* thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
			if (!(thisWindow->OnCursorPosCallback)) return;

			int LMouseButtonState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
			if (LMouseButtonState == GLFW_PRESS)
				thisWindow->OnCursorPosCallback(true, xpos, ypos); // pressed: true
			else if (LMouseButtonState == GLFW_RELEASE)
				thisWindow->OnCursorPosCallback(false, xpos, ypos); // pressed: false
			});

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigWindowsMoveFromTitleBarOnly = true;
		// just like my vs2022
		io.Fonts->AddFontFromFileTTF("CascadiaMono.ttf", 20.f);
		io.FontDefault = io.Fonts->Fonts.back();

		ImGui_ImplGlfw_InitForOpenGL(m_GlfwWindow, true);
		ImGui_ImplOpenGL3_Init(glsl_version);
	}

	void Window::Destroy()
	{
		ImGui_ImplGlfw_Shutdown();
		ImGui_ImplOpenGL3_Shutdown();
		ImGui::DestroyContext();

		glfwDestroyWindow(m_GlfwWindow);
		glfwTerminate();
	}

	int Window::PrepareFrame()
	{
		glfwPollEvents();
		
		/* clear previous frame data, because other project manipulate the 2d texture resources */
		int display_w, display_h;
		glfwGetFramebufferSize(m_GlfwWindow, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClear(GL_COLOR_BUFFER_BIT);

		// TODO: mac error can't get backend data from opengl3
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (OnRenderFrame) {
			OnRenderFrame(this);
		}

		return glfwWindowShouldClose(m_GlfwWindow);
	}

	void Window::SwapWindow()
	{
		// output to renderer
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// update delta time
		m_CurrentFrame = glfwGetTime();
		m_DeltaTime = m_CurrentFrame - m_LastFrame;
		m_LastFrame = m_CurrentFrame;

		// cap fps
		long msToSleep = (1000.0 / MaxFPS) - (m_DeltaTime * 100.0);
		if (msToSleep > 0.0)
			std::this_thread::sleep_for(std::chrono::milliseconds(msToSleep - 1));

		// swap window buffers
		glfwSwapBuffers(m_GlfwWindow);
	}

	/* * * * * * * * *
	* Getter & Setter
	*/

	GLFWwindow* Window::GetGlfwWindow()
	{
		return m_GlfwWindow;
	}

	void Window::SetWindowOpacity(float value)
	{
		glfwSetWindowOpacity(m_GlfwWindow, value);
	}

	void Window::GetWindowSize(int* outWidth, int* outHeight)
	{
		glfwGetWindowSize(m_GlfwWindow, outWidth, outHeight);
	}

	double Window::GetDeltaTime() const
	{
		return m_DeltaTime;
	}
}