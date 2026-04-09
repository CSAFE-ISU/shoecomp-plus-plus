#include "ui/splash.h"
#include "ui/settings.h"
#include "ui/uiHelpers.h"
#include "hello_imgui/hello_imgui.h"
#include "hello_imgui/imgui_theme.h"
#include "formats/png.h"
#include "imgui.h"
#include "stb_image.h"

namespace shoecomp
{
    SplashResult runSplash(double duration, int progressSteps)
    {
        SplashResult result;
        double startTime = 0.0;
        ImFont* titleFont = nullptr;
        ImFont* smallFont = nullptr;
        ImTextureID iconTexture = 0;
        int iconWidth = 0;
        int iconHeight = 0;

        HelloImGui::RunnerParams params;
        params.appWindowParams.windowTitle = "shoecomp";
        params.appWindowParams.windowGeometry.size = {576, 324};  // 10% smaller
        params.appWindowParams.borderless = true;
        params.appWindowParams.borderlessMovable = false;
        params.appWindowParams.borderlessResizable = false;
        params.appWindowParams.borderlessClosable = true;
        params.appWindowParams.resizable = false;
        params.appWindowParams.windowGeometry.positionMode =
            HelloImGui::WindowPositionMode::MonitorCenter;
        params.imGuiWindowParams.defaultImGuiWindowType =
            HelloImGui::DefaultImGuiWindowType::ProvideFullScreenWindow;
        params.callbacks.LoadAdditionalFonts = [&]()
        {
            titleFont = HelloImGui::LoadFont(
                "fonts/Montserrat-SemiBold.ttf", 24.0f);
            smallFont = HelloImGui::LoadFont(
                "fonts/Inconsolata-Regular.ttf", 14.0f);
        };
        params.callbacks.PostInit = [&]()
        {
            ImGui::GetIO().FontGlobalScale = 2.5f;
            applyTheme(2);  // Material Flat

            // Load icon from internal assets
            auto iconData = HelloImGui::LoadAssetFileData("fonts/icon.png");
            if (iconData.data)
            {
                int w = 0, h = 0, channels = 0;
                unsigned char* imageData = stbi_load_from_memory(
                    (const unsigned char*)iconData.data,
                    (int)iconData.dataSize,
                    &w, &h, &channels, 4);

                if (imageData)
                {
                    iconTexture = createTextureRGBA(imageData, w, h);
                    iconWidth = w;
                    iconHeight = h;
                    stbi_image_free(imageData);
                }

                HelloImGui::FreeAssetFileData(&iconData);
            }
        };
        params.callbacks.ShowGui = [&]()
        {
            if (startTime == 0.0) startTime = ImGui::GetTime();

            double elapsed = ImGui::GetTime() - startTime;

            ImVec2 winSize = ImGui::GetWindowSize();
            // Get base font size (before global scale is applied)
            float fontSize = ImGui::GetFontSize() / ImGui::GetIO().FontGlobalScale;

            // Check for window close or ESC key
            if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                HelloImGui::GetRunnerParams()->appShallExit)
            {
                result.cancelled = true;
                HelloImGui::GetRunnerParams()->appShallExit = true;
            }

            // X button in top-right corner - transparent background, visible on hover
            ImVec4 buttonColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);  // Fully transparent
            ImVec4 buttonHovered = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
            buttonHovered.w = 0.4f;  // Semi-transparent on hover
            ImVec4 buttonActive = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
            buttonActive.w = 0.6f;

            ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonActive);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

            // Button size relative to font
            float buttonSize = fontSize * kSplashButtonSize;
            ImGui::SetCursorPos(ImVec2(winSize.x - buttonSize - fontSize * kSplashButtonOffsetX,
                                      fontSize * kSplashButtonOffsetY));
            if (ImGui::Button("x", ImVec2(buttonSize, buttonSize * 1.25f)))
            {
                result.cancelled = true;
                HelloImGui::GetRunnerParams()->appShallExit = true;
            }

            ImGui::PopStyleVar(1);
            ImGui::PopStyleColor(3);

            // Left-aligned content
            float leftMargin = fontSize * kSplashLeftMargin;
            float centerY = winSize.y * 0.5f;

            // Calculate icon display size
            float iconDisplaySize = fontSize * kSplashIconSize;
            float iconSpacing = fontSize * kSplashIconSpacing;

            // Title "shoecomp" (lowercase) - left aligned
            if (titleFont) ImGui::PushFont(titleFont);
            const char* title = "shoecomp";
            ImVec2 titleSize = ImGui::CalcTextSize(title);
            if (titleFont) ImGui::PopFont();

            // Progress bar width
            float progressBarWidth = titleSize.x * 1.5f;

            // Icon to the left
            if (iconTexture)
            {
                ImGui::SetCursorPos(
                    ImVec2(leftMargin,
                           centerY - iconDisplaySize * 0.5f));
                ImGui::Image(iconTexture, ImVec2(iconDisplaySize, iconDisplaySize));
            }

            // Title - to the right of icon, left aligned
            if (titleFont) ImGui::PushFont(titleFont);
            ImGui::SetCursorPos(
                ImVec2(leftMargin + iconDisplaySize + iconSpacing,
                       centerY - fontSize * kSplashTitleOffsetY));
            ImGui::Text("%s", title);
            if (titleFont) ImGui::PopFont();

            // Progress bar - left aligned with title
            float progress = (float)(elapsed / duration);
            int currentStep = (int)(progress * progressSteps);
            if (currentStep > progressSteps)
                currentStep = progressSteps;

            float progressBarHeight = fontSize * kSplashProgressHeight;

            ImGui::SetCursorPos(
                ImVec2(leftMargin + iconDisplaySize + iconSpacing,
                       centerY + fontSize * kSplashProgressOffsetY));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
            ImGui::PushItemWidth(progressBarWidth);
            ImGui::ProgressBar(progress, ImVec2(-1.0f, progressBarHeight), "");
            ImGui::PopItemWidth();
            ImGui::PopStyleColor(2);

            // Step text below progress bar - left aligned
            char stepText[32];
            snprintf(stepText, sizeof(stepText), "Step %d/%d",
                     currentStep, progressSteps);

            if (smallFont) ImGui::PushFont(smallFont);
            ImGui::SetCursorPos(
                ImVec2(leftMargin + iconDisplaySize + iconSpacing,
                       centerY + fontSize * kSplashStepTextOffsetY));
            ImGui::TextDisabled("%s", stepText);
            if (smallFont) ImGui::PopFont();

            if (elapsed >= duration)
                HelloImGui::GetRunnerParams()->appShallExit = true;
        };

        HelloImGui::Run(params);

        return result;
    }
}
