#include "ExitPopup.h"
#include <stdio.h>

namespace GLUI {
// TODO: make class?

	/* this popup UI properties */
	bool is_exit_popup = false;
	const char* exit_popup_text = "Close remining";
	const float exit_timeout = 3.f;
	float popup_time = .0f;
	GLFWwindow* window;
	GLFWwindowclosefun close_callback = nullptr;

	void exit_popup_Init(GLFWwindow* wnd, GLFWwindowclosefun callback) {
		window = wnd;
		close_callback = callback;
	}

	void exit_popup_Update(bool show) {
		is_exit_popup = show;
		// is showing for remaining close, TODO: or design cancellation?
		popup_time = show ? ImGui::GetTime() : .0f;
	}

	void exit_popup_Render() {
		if (is_exit_popup == false) {
			return;
		}
		if (!window) {
			printf("ERROR: init before using");
			return;
		}
		// at least set popup enable time to calculate remining
		if (popup_time == .0f) {
			printf("ERROR: at least calls Update once!");
			return;
		}
		auto current_time = ImGui::GetTime();
		// calculate remain time from begins of showing popup to current time
		auto remaining = exit_timeout - (current_time - popup_time);
		ImGui::OpenPopup(exit_popup_text);
		if (ImGui::BeginPopupModal(exit_popup_text, &is_exit_popup, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Closing in %.1f seconds...", remaining);
			if (remaining <= 0.f) {
				// close this popup
				ImGui::CloseCurrentPopup();
				// invoke main window custom close callback
				if (close_callback) {
					close_callback(window);
				}
				// exit message loop to close main window
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
			ImGui::EndPopup();
		}
	}
}