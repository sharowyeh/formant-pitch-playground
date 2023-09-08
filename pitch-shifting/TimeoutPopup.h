#pragma once
/* for GLFWwindow to set main window close */
#include "../imgui/imgui_impl_glfw.h"
#include <GLFW/glfw3.h>
#include <string>

using std::string;

namespace GLUI {

class TimeoutPopup {
public:
	TimeoutPopup(GLFWwindow* window);
	~TimeoutPopup();
	/* design for caller custom close callback */
	void(*OnTimeoutElapsed)(GLFWwindow* window);

	void Show(bool show = true, float timeout = 3.f);
	void Render();
private:
	GLFWwindow* parentWindow;
	const char* popupName;
	bool isShow;
	string content;
	float timeout;
	float elapsed;
};
}