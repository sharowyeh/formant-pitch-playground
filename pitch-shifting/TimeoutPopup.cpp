#include "TimeoutPopup.h"
#include "../imgui/imgui.h"
#include <stdio.h>

namespace GLUI {

	TimeoutPopup::TimeoutPopup(GLFWwindow* window):
		popupName("##ConfirmClose"),
		isShow(false),
		timeout(3.f),
		elapsed(0.f),
		OnTimeoutElapsed(nullptr)
	{
		parentWindow = window;
	}

	TimeoutPopup::~TimeoutPopup()
	{
	}

	void TimeoutPopup::Show(bool show, float timeout)
	{
		if (isShow != show) { // sort of reset elapsed time
			elapsed = 0.f;
		}
		isShow = show;
		this->timeout = timeout;
	}

	void TimeoutPopup::Render()
	{
		if (isShow == false) {
			return;
		}
		if (!parentWindow) {
			printf("require parent window\n");
			return;
		}
		// at least set popup enable time to calculate remining
		if (timeout == .0f) {
			printf("meaningless without timeout!\n");
			return;
		}
		// previous design was getting timespan from enable popup time to ImGui::GetTime() 
		elapsed += ImGui::GetIO().DeltaTime;
		auto current_time = ImGui::GetTime();
		// calculate remain time from begins of showing popup to current time
		auto remaining = timeout - elapsed;
		ImGui::OpenPopup(popupName);
		if (ImGui::BeginPopupModal(popupName, &isShow, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Closing in %.1f seconds...", remaining);
			if (remaining <= 0.f) {
				// one shot
				elapsed = 0.f;
				// close this popup
				ImGui::CloseCurrentPopup();
				// invoke caller custom close callback
				if (OnTimeoutElapsed) {
					OnTimeoutElapsed(parentWindow);
				}
			}
			ImGui::EndPopup();
		}
	}
}