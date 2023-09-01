#pragma once
/* for GLFWwindow to set main window close */
#include "../imgui/imgui_impl_glfw.h"
#include <GLFW/glfw3.h>

namespace GLUI {
	void exit_popup_Init(GLFWwindow* wnd, GLFWwindowclosefun callback);
	void exit_popup_Update(bool show);
	void exit_popup_Render();
}