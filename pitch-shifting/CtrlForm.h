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
	
	void(*OnButtonClicked)(string name, int param) = nullptr;

	void SetAudioDeviceList(std::vector<PitchShifting::SourceDesc>& devices, int defInDevIndex = -1, int defOutDevIndex = -1);

	void Render();
private:
	GLFWwindow* parentWindow;
	const char* ctrlFormName;
};

} // namespace GLUI