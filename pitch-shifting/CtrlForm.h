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
struct ButtonEventArgs : GLUIEventArgs<int, void*> {
	ButtonEventArgs() : GLUIEventArgs(-1, nullptr) { }
	ButtonEventArgs(int id, void* arg) : GLUIEventArgs(id, arg) { }
};

enum CtrlFormIds : int {
	SetInputDeviceButton = 1,
	SetOutputDeviceButton,
	SetInputFileButton,
	SetAdjustmentButton
};

struct CtrlFormData {
	PitchShifting::SourceDesc InputSource;
	PitchShifting::SourceDesc OutputSource;
	double PitchShift = 0.f;
	double FormantShift = 0.f;
	double InputGain = 0.f;
	CtrlFormData() : 
		InputSource(PitchShifting::SourceDesc()),
		OutputSource(PitchShifting::SourceDesc())
	{ };
};

class CtrlForm {
public:
	CtrlForm(GLFWwindow* window);
	~CtrlForm();
	
	CtrlFormData FormData;

	void(*OnButtonClicked)(void* inst, ButtonEventArgs param) = nullptr;

	void SetAudioDeviceList(std::vector<PitchShifting::SourceDesc>& devices, int defInDevIndex = -1, int defOutDevIndex = -1);
	/* TODO: sofar useless, should define event args from UI control event callbacks */
	void GetAudioSources(int* inDevIndex, int* outDevIndex);

	void SetAudioFileList(std::vector<PitchShifting::SourceDesc>& files, char* defInFile = nullptr, char* defOutFile = nullptr);
	/* TODO: sofar useless, should define event args from UI control event callbacks */
	void GetAudioFiles(char* inFile, char* outFile);

	void SetAudioAdjustment(double pitch, double formant, double inputGain);
	void GetAudioAdjustment(double* pitch, double* formant, double* inputGain);

	void Render();
private:
	GLFWwindow* parentWindow;
	const char* ctrlFormName;
};

} // namespace GLUI