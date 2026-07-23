#pragma once

#include "third-party/imgui/imgui.h"

namespace tracker {

inline ImVec4 rgb(int r, int g, int b, int a = 255) {
  return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

inline const ImVec4 TAN = rgb(0xB6, 0x99, 0x67);
inline const ImVec4 GADGET = rgb(0xC6, 0xAA, 0x78);
inline const ImVec4 GADGET_HI = rgb(0xDC, 0xC4, 0x96);
inline const ImVec4 GADGET_LO = rgb(0x92, 0x72, 0x44);
inline const ImVec4 INSET = rgb(0x9C, 0x7E, 0x4E);
inline const ImVec4 BROWN = rgb(0x53, 0x3C, 0x20);
inline const ImVec4 CREAM = rgb(0xE6, 0xD4, 0xA8);
inline const ImVec4 INK = rgb(0x18, 0x10, 0x08);
inline const ImVec4 INK_DIM = rgb(0x6E, 0x56, 0x36);

inline const ImVec4 SCREEN_BG = rgb(0x00, 0x00, 0x00);
inline const ImVec4 SCREEN_ROW = rgb(0x08, 0x10, 0x1C);
inline const ImVec4 SCREEN_FG = rgb(0x2E, 0x82, 0xCE);
inline const ImVec4 SCREEN_DIM = rgb(0x1E, 0x50, 0x84);
inline const ImVec4 SCOPE = rgb(0x46, 0xAC, 0xF0);

inline const ImVec4 ACCENT = rgb(0x2E, 0x82, 0xCE);
inline const ImVec4 ACCENT_BRIGHT = rgb(0x4C, 0xA0, 0xE4);

inline void apply_style() {
  ImGuiStyle& s = ImGui::GetStyle();

  s.WindowRounding = 0.0f;
  s.ChildRounding = 0.0f;
  s.FrameRounding = 0.0f;
  s.PopupRounding = 0.0f;
  s.ScrollbarRounding = 0.0f;
  s.GrabRounding = 0.0f;
  s.TabRounding = 0.0f;

  s.WindowBorderSize = 1.0f;
  s.ChildBorderSize = 1.0f;
  s.FrameBorderSize = 1.0f;
  s.PopupBorderSize = 1.0f;

  s.WindowPadding = ImVec2(8, 8);
  s.FramePadding = ImVec2(7, 4);
  s.ItemSpacing = ImVec2(7, 5);
  s.ItemInnerSpacing = ImVec2(5, 4);
  s.ScrollbarSize = 16.0f;
  s.GrabMinSize = 14.0f;
  s.WindowTitleAlign = ImVec2(0.5f, 0.5f);

  ImVec4* c = s.Colors;
  c[ImGuiCol_Text] = INK;
  c[ImGuiCol_TextDisabled] = INK_DIM;
  c[ImGuiCol_WindowBg] = TAN;
  c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_PopupBg] = TAN;
  c[ImGuiCol_Border] = BROWN;
  c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_FrameBg] = INSET;
  c[ImGuiCol_FrameBgHovered] = rgb(0xAE, 0x90, 0x5E);
  c[ImGuiCol_FrameBgActive] = rgb(0x86, 0x68, 0x3C);
  c[ImGuiCol_TitleBg] = GADGET_LO;
  c[ImGuiCol_TitleBgActive] = GADGET;
  c[ImGuiCol_TitleBgCollapsed] = GADGET_LO;
  c[ImGuiCol_MenuBarBg] = TAN;
  c[ImGuiCol_ScrollbarBg] = rgb(0x8A, 0x6C, 0x40);
  c[ImGuiCol_ScrollbarGrab] = GADGET;
  c[ImGuiCol_ScrollbarGrabHovered] = GADGET_HI;
  c[ImGuiCol_ScrollbarGrabActive] = ACCENT;
  c[ImGuiCol_CheckMark] = ACCENT;
  c[ImGuiCol_SliderGrab] = ACCENT;
  c[ImGuiCol_SliderGrabActive] = ACCENT_BRIGHT;
  c[ImGuiCol_Button] = GADGET;
  c[ImGuiCol_ButtonHovered] = GADGET_HI;
  c[ImGuiCol_ButtonActive] = GADGET_LO;
  c[ImGuiCol_Header] = CREAM;
  c[ImGuiCol_HeaderHovered] = rgb(0xD2, 0xBC, 0x8C);
  c[ImGuiCol_HeaderActive] = rgb(0xF0, 0xE2, 0xBE);
  c[ImGuiCol_Separator] = BROWN;
  c[ImGuiCol_SeparatorHovered] = ACCENT;
  c[ImGuiCol_SeparatorActive] = ACCENT_BRIGHT;
  c[ImGuiCol_ResizeGrip] = GADGET;
  c[ImGuiCol_ResizeGripHovered] = GADGET_HI;
  c[ImGuiCol_ResizeGripActive] = ACCENT;
  c[ImGuiCol_PlotHistogram] = ACCENT;
  c[ImGuiCol_PlotHistogramHovered] = ACCENT_BRIGHT;
  c[ImGuiCol_PlotLines] = ACCENT;
  c[ImGuiCol_TableHeaderBg] = SCREEN_ROW;
  c[ImGuiCol_TableBorderStrong] = BROWN;
  c[ImGuiCol_TableBorderLight] = rgb(0x12, 0x1A, 0x28);
  c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_TableRowBgAlt] = rgb(0xFF, 0xFF, 0xFF, 0x0A);
  c[ImGuiCol_NavHighlight] = ACCENT;
}

}  // namespace tracker
