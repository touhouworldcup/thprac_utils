#include "window.h"


int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ PWSTR pCmdLine,
	_In_ int nCmdShow)
{
	// Supresses C4100 (unreferenced formal parameter(s))
	hPrevInstance;
	pCmdLine;
	nCmdShow;

	if (!GuiWndInit(hInstance, L"thprac devtools", L"thprac devtools", 640, 480, 1280, 960)) {
		return 1;
	}

	bool isOpen = true;
	bool isMinimize = false;

	while (GuiNewFrame()) {
		ImGui::SetNextWindowSizeConstraints(ImVec2(640.0f, 480.0f), ImVec2(1280.0f, 960.0f));
		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(960.0f, 720.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(0.9f);

		bool canMove = false;

		ImGui::Begin("thprac devtools", &isOpen, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove, &isMinimize);

		if (!isOpen)
			break;
		if (isMinimize) {
			isMinimize = false;
			GuiMinimizeWindow();
		}
		canMove = ImGui::IsItemHovered();

		static char exeSig[2048] = {};

		if (ImGui::BeginTabBar("MenuTabBar")) {
			if (GuiTabItem(
				"Generate locale_def.(h/cpp) from a JSON file"
			)) {
				extern void loc_json_gui();
				loc_json_gui();
				ImGui::EndTabItem();
			}
			if (GuiTabItem("Generate a template for an exe signature")) {
				extern void exe_sig_gui();
				exe_sig_gui();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImVec2 wndPos = ImGui::GetWindowPos();
		ImVec2 wndSize = ImGui::GetWindowSize();
		canMove &= !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive();
		ImGui::End();
		GuiEndFrame(wndPos, wndSize, canMove);
	}

	return 0;
}
