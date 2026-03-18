#include "ui/alignDialog.h"
#include "calc/align.h"
#include "imgui.h"

namespace shoecomp
{
    bool AlignDialog::render()
    {
        if (show)
        {
            show = false;
            open = true;
            ImGui::OpenPopup("Align Images");
        }

        if (!open) return false;

        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(
            ImVec2(ds.x * 0.4f, ds.y * 0.5f),
            ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(
            ImVec2(ds.x * 0.3f, ds.y * 0.25f),
            ImGuiCond_Appearing);

        bool accepted = false;

        if (ImGui::BeginPopupModal(
                "Align Images", nullptr,
                ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("Aligning %s to %s",
                        rightName.c_str(),
                        leftName.c_str());
            ImGui::Separator();

            int modeInt = (int)mode;
            AlignMode prevMode = mode;
            ImGui::RadioButton("Manual", &modeInt, 0);
            ImGui::SameLine();
            ImGui::RadioButton(
                "Automatic", &modeInt, 1);
            mode = (AlignMode)modeInt;

            if (mode == AlignMode::Automatic)
            {
                // Drain messages
                while (auto msg =
                           channel.messages.pop())
                {
                    if (msg->kind ==
                        MsgKind::Done)
                        workerFinished = true;
                    if (msg->kind ==
                        MsgKind::Cancelled)
                        workerFinished = false;
                }

                bool running =
                    channel.is_running.load();

                ImGui::Spacing();
                if (!running && !workerFinished)
                {
                    if (ImGui::Button("Run"))
                        startWorker();
                }
                else if (running)
                {
                    float p =
                        channel.progress.load();
                    ImGui::ProgressBar(p);
                    if (ImGui::Button(
                            "Cancel##worker"))
                        cancelWorker();
                }
                else
                {
                    ImGui::Text(
                        "Alignment complete.");
                }
            }

            if (mode != prevMode)
                ImGui::SetNextItemOpen(
                    mode == AlignMode::Manual);
            if (ImGui::CollapsingHeader("Transform"))
            {
                bool disabled =
                    mode == AlignMode::Automatic;
                if (disabled) ImGui::BeginDisabled();

                ImGui::SliderFloat(
                    "Rotation (deg)",
                    &result.rotation,
                    -180.0f, 180.0f, "%.1f");
                ImGui::SliderFloat(
                    "Translation X",
                    &result.translationX,
                    -2000.0f, 2000.0f, "%.1f");
                ImGui::SliderFloat(
                    "Translation Y",
                    &result.translationY,
                    -2000.0f, 2000.0f, "%.1f");
                ImGui::SliderFloat(
                    "Scale", &result.scale,
                    0.1f, 10.0f, "%.2f");

                if (disabled) ImGui::EndDisabled();
            }

            ImGui::Separator();

            if (ImGui::Button("OK"))
            {
                cancelWorker();
                cleanup();
                accepted = true;
                open = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                cancelWorker();
                cleanup();
                open = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        return accepted;
    }

    void AlignDialog::startWorker()
    {
        cleanup();
        channel.cancel_requested.store(false);
        channel.is_running.store(true);
        channel.progress.store(0.0f);
        workerFinished = false;

        auto left = leftImage;
        auto right = rightImage;
        workerThread = std::thread(
            [this, left, right]()
            {
                runAutoAlign(
                    *left, *right,
                    channel, result);
            });
    }

    void AlignDialog::cancelWorker()
    {
        channel.cancel_requested.store(true);
    }

    void AlignDialog::cleanup()
    {
        if (workerThread.joinable())
            workerThread.join();
    }

}  // namespace shoecomp
