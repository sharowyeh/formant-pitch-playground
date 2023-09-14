#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include "stretcher.hpp" // for device desc structure

using std::string;

namespace GLUI {

class CtrlForm {
public:
	CtrlForm(GLFWwindow* window);
	~CtrlForm();

	void SetInputSourceList(std::vector<PitchShifting::SourceDesc> devices, int defaultSelection = -1);
	void SetOutputSourceList(std::vector<PitchShifting::SourceDesc> devices, int defaultSelection = -1);

	void Render();
private:
	GLFWwindow* parentWindow;
	const char* ctrlFormName;
};

} // namespace GLUI