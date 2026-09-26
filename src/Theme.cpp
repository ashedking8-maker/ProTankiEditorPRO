#include "Theme.h"
#include <imgui.h>
#include <algorithm>

void ApplyGTanksTheme(int variant, const std::array<float,3>& surfaces, const std::array<float,3>& text) {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 2.5f;
    s.ChildRounding = 2.0f;
    s.FrameRounding = 2.0f;
    s.PopupRounding = 2.5f;
    s.GrabRounding = 2.0f;
    s.TabRounding = 2.0f;
    s.ScrollbarRounding = 2.0f;
    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize = 0.0f;
    s.PopupBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.TabBorderSize = 0.0f;
    s.WindowPadding = {8, 7};
    s.FramePadding = {7, 4};
    s.ItemSpacing = {7, 5};
    s.ItemInnerSpacing = {5, 4};
    s.IndentSpacing = 14.0f;
    s.ScrollbarSize = 11.0f;
    s.GrabMinSize = 7.0f;

    auto& c = s.Colors;
    c[ImGuiCol_Text]                 = {0.84f,0.86f,0.88f,1.00f};
    c[ImGuiCol_TextDisabled]         = {0.45f,0.47f,0.50f,1.00f};
    c[ImGuiCol_WindowBg]             = {0.047f,0.052f,0.060f,0.95f};
    c[ImGuiCol_ChildBg]              = {0.047f,0.052f,0.060f,0.82f};
    c[ImGuiCol_PopupBg]              = {0.055f,0.060f,0.070f,0.96f};
    c[ImGuiCol_Border]               = {0.20f,0.22f,0.25f,0.60f};
    c[ImGuiCol_FrameBg]              = {0.10f,0.11f,0.13f,0.72f};
    c[ImGuiCol_FrameBgHovered]       = {0.15f,0.16f,0.19f,0.86f};
    c[ImGuiCol_FrameBgActive]        = {0.18f,0.20f,0.23f,0.94f};
    c[ImGuiCol_TitleBg]              = {0.045f,0.050f,0.058f,1.00f};
    c[ImGuiCol_TitleBgActive]        = {0.060f,0.066f,0.076f,1.00f};
    c[ImGuiCol_MenuBarBg]            = {0.052f,0.058f,0.066f,0.94f};
    c[ImGuiCol_Button]               = {0.11f,0.12f,0.14f,0.66f};
    c[ImGuiCol_ButtonHovered]        = {0.17f,0.18f,0.21f,0.84f};
    c[ImGuiCol_ButtonActive]         = {0.20f,0.23f,0.27f,0.96f};
    c[ImGuiCol_Header]               = {0.12f,0.16f,0.20f,0.74f};
    c[ImGuiCol_HeaderHovered]        = {0.16f,0.21f,0.27f,0.86f};
    c[ImGuiCol_HeaderActive]         = {0.19f,0.27f,0.35f,0.94f};
    c[ImGuiCol_CheckMark]            = {0.34f,0.58f,0.82f,1.00f};
    c[ImGuiCol_SliderGrab]           = {0.34f,0.58f,0.82f,0.85f};
    c[ImGuiCol_SliderGrabActive]     = {0.44f,0.68f,0.94f,1.00f};
    c[ImGuiCol_Tab]                  = {0.08f,0.09f,0.105f,0.92f};
    c[ImGuiCol_TabHovered]           = {0.16f,0.20f,0.25f,0.96f};
    c[ImGuiCol_TabSelected]          = {0.12f,0.16f,0.20f,1.00f};
    c[ImGuiCol_Separator]            = {0.22f,0.24f,0.27f,0.38f};
    c[ImGuiCol_ResizeGrip]           = {0.34f,0.58f,0.82f,0.15f};
    c[ImGuiCol_ResizeGripHovered]    = {0.34f,0.58f,0.82f,0.45f};
    c[ImGuiCol_DockingPreview]       = {0.30f,0.55f,0.82f,0.30f};
    if (variant == 1 || variant == 2 || variant == 3) {
        // Tint SURFACES only: text, check marks, handles and selection accents
        // remain unchanged across themes. This prevents illegible green text.
        const ImGuiCol surfaceColors[] = {
            ImGuiCol_WindowBg, ImGuiCol_ChildBg, ImGuiCol_PopupBg,
            ImGuiCol_Border, ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered,
            ImGuiCol_FrameBgActive, ImGuiCol_TitleBg, ImGuiCol_TitleBgActive,
            ImGuiCol_MenuBarBg, ImGuiCol_Button, ImGuiCol_ButtonHovered,
            ImGuiCol_ButtonActive, ImGuiCol_Header, ImGuiCol_HeaderHovered,
            ImGuiCol_HeaderActive, ImGuiCol_Tab, ImGuiCol_TabHovered,
            ImGuiCol_TabSelected, ImGuiCol_Separator
        };
        for (const ImGuiCol index : surfaceColors) {
            auto& v=c[index];
            const float luminance=0.28f*v.x+0.61f*v.y+0.11f*v.z;
            if (variant == 1) {
                v.x=std::clamp(luminance*0.72f,0.0f,1.0f);
                v.y=std::clamp(luminance*1.18f,0.0f,1.0f);
                v.z=std::clamp(luminance*0.79f,0.0f,1.0f);
            } else if (variant == 2) {
                // Restrained silver panel, with white text kept legible.
                const float silver=std::clamp(0.10f+luminance*0.70f,0.0f,0.33f);
                v.x=silver;v.y=std::min(1.0f,silver+0.012f);
                v.z=std::min(1.0f,silver+0.025f);
            } else {
                // Selected RGB changes the surfaces, not text or scene materials.
                const float brightness=std::clamp(0.32f+luminance*3.0f,0.12f,0.98f);
                v.x=std::clamp(surfaces[0]*brightness,0.f,1.f);
                v.y=std::clamp(surfaces[1]*brightness,0.f,1.f);
                v.z=std::clamp(surfaces[2]*brightness,0.f,1.f);
            }
        }
        if (variant == 2) {
            c[ImGuiCol_Text]={0.91f,0.93f,0.95f,1.0f};
            c[ImGuiCol_TextDisabled]={0.66f,0.69f,0.72f,1.0f};
        } else if (variant == 3) {
            c[ImGuiCol_Text]={text[0],text[1],text[2],1.0f};
            c[ImGuiCol_TextDisabled]={text[0]*0.65f,text[1]*0.65f,text[2]*0.65f,1.0f};
        }
    }

}
