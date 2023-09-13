#pragma once

#include <GLFW/glfw3.h>
#include <string>

using std::string;

namespace GLUI {

class CtrlForm {
public:
	CtrlForm(GLFWwindow* window);
	~CtrlForm();

	void Render();
private:
	GLFWwindow* parentWindow;
	const char* ctrlFormName;
};

} // namespace GLUI