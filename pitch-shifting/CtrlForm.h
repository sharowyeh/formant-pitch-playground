#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include "stretcher.hpp" // for device desc structure

using std::string;

namespace GLUI {

// DEBUG: try to design a templatable struct for UI event callbacks
template<typename... args>
struct GLUIEventArgs {
	std::tuple<args...> data;
	template<typename... T>
	GLUIEventArgs(T&&... t) : data(std::forward<T>(t)...) {

	}
};

// DEBUG: need to define templating type of given args for GLUIEventArgs
struct ButtonEventArgs : GLUIEventArgs<string, int> {
	ButtonEventArgs() : GLUIEventArgs("", -1) { }
	ButtonEventArgs(string a, int b) : GLUIEventArgs(a, b) { }
};

class CtrlForm {
public:
	CtrlForm(GLFWwindow* window);
	~CtrlForm();
	
	void(*OnButtonClicked)(void* inst, ButtonEventArgs param) = nullptr;

	void SetAudioDeviceList(std::vector<PitchShifting::SourceDesc>& devices, int defInDevIndex = -1, int defOutDevIndex = -1);
	/* get device index of current selected audio source */
	void GetAudioSources(int* inDevIndex, int* outDevIndex);

	void Render();
private:
	GLFWwindow* parentWindow;
	const char* ctrlFormName;
};

} // namespace GLUI