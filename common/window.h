#pragma once
#include <Windows.h>
#include <d3d9.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx9.h>

void GuiMinimizeWindow();
bool GuiNewFrame();
bool GuiTabItem(const char* tabText);
bool GuiEndFrame(ImVec2& wndPos, ImVec2& wndSize, bool canMove);
bool GuiWndInit(HINSTANCE hInstance, LPCWSTR className, LPCWSTR title, unsigned int width, unsigned int height, unsigned int maxWidth, unsigned int maxHeight);
HWND GuiGetWindow();