#include "ui/alignDialog.h"
#include "ui/imageCanvas.h"
#include "calc/align.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace shoecomp
{
    AlignDialogResult AlignDialog::render(
        const std::vector<AlignState>& aligns, int alignIdx)
    {
        if (show)
        {
            show = false;
            open = true;
            ImGui::OpenPopup("Align Images");
        }

        if (!open) return AlignDialogResult::None;

        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(ds.x * 0.5f, ds.y * 0.6f),
                                 ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.25f, ds.y * 0.2f),
                                ImGuiCond_Appearing);

        AlignDialogResult dialogResult = AlignDialogResult::None;

        if (ImGui::BeginPopupModal(
                "Align Images", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("Aligning %s to %s", rightName.c_str(),
                        leftName.c_str());
            ImGui::Separator();

            if (ImGui::BeginTabBar("##AlignTabs"))
            {
                if (ImGui::BeginTabItem("Manual"))
                {
                    mode = AlignMode::Manual;

                    // Alignment picker
                    if (!aligns.empty())
                    {
                        int idx = alignIdx;
                        char preview[128];
                        snprintf(preview, sizeof(preview),
                                 "%d: R:%.1f"
                                 " T:(%.0f,%.0f)"
                                 " S:%.2f",
                                 idx + 1, aligns[idx].rotation,
                                 aligns[idx].translationX,
                                 aligns[idx].translationY,
                                 aligns[idx].scale);
                        ImGui::SetNextItemWidth(
                            ImGui::GetContentRegionAvail().x - 80.0f);
                        if (ImGui::BeginCombo("##AlignPicker", preview))
                        {
                            for (int i = 0; i < (int)aligns.size(); ++i)
                            {
                                char label[128];
                                snprintf(label, sizeof(label),
                                         "%d: R:%.1f"
                                         " T:(%.0f,"
                                         "%.0f)"
                                         " S:%.2f",
                                         i + 1, aligns[i].rotation,
                                         aligns[i].translationX,
                                         aligns[i].translationY,
                                         aligns[i].scale);
                                bool selected = (i == idx);
                                if (ImGui::Selectable(label, selected))
                                {
                                    current.rotation =
                                        aligns[i].rotation;
                                    current.translationX =
                                        aligns[i].translationX;
                                    current.translationY =
                                        aligns[i].translationY;
                                    current.scale = aligns[i].scale;
                                }
                                if (selected)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::Spacing();
                    }

                    ImGui::SliderFloat("Rotation (deg)",
                                       &current.rotation, -180.0f,
                                       180.0f, "%.1f");
                    ImGui::SliderFloat("Translation X",
                                       &current.translationX, -2000.0f,
                                       2000.0f, "%.1f");
                    ImGui::SliderFloat("Translation Y",
                                       &current.translationY, -2000.0f,
                                       2000.0f, "%.1f");
                    ImGui::SliderFloat("Scale", &current.scale, 0.1f,
                                       10.0f, "%.2f");

                    ImGui::Spacing();
                    ImGui::Separator();
                    if (ImGui::Button("Add"))
                    {
                        cancelWorker();
                        dialogResult = AlignDialogResult::Add;
                        open = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Replace"))
                    {
                        cancelWorker();
                        dialogResult = AlignDialogResult::Replace;
                        open = false;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Automatic"))
                {
                    mode = AlignMode::Automatic;

                    int nw = static_cast<int>(rtsParams.numWorkers);
                    if (ImGui::SliderInt("Workers", &nw, 1, 8))
                    {
                        rtsParams.numWorkers = static_cast<size_t>(nw);
                    }

                    int nr = static_cast<int>(rtsParams.numResults);
                    if (ImGui::InputInt("Results", &nr))
                    {
                        if (nr < 1) nr = 1;
                        rtsParams.numResults = static_cast<size_t>(nr);
                    }

                    // Compute max points from
                    // either image
                    int maxPts = 256;
                    auto countPts =
                        [](const std::shared_ptr<ImageData>& img) -> int
                    {
                        if (!img) return 0;
                        auto& a = img->annotations;
                        if (!a.isObject() || !a.contains("points") ||
                            !a["points"].isArray())
                            return 0;
                        return static_cast<int>(
                            a["points"].getArray().size());
                    };
                    int lp = countPts(leftImage);
                    int rp = countPts(rightImage);
                    maxPts = std::max({lp, rp, 3});
                    rtsParams.upperBound = maxPts;

                    ImGui::SliderInt("Lower Bound",
                                     &rtsParams.lowerBound, 3, maxPts);
                    if (rtsParams.lowerBound < 3)
                        rtsParams.lowerBound = 3;

                    ImGui::SliderInt("Upper Bound",
                                     &rtsParams.upperBound,
                                     rtsParams.lowerBound, maxPts);
                    if (rtsParams.upperBound < rtsParams.lowerBound)
                        rtsParams.upperBound = rtsParams.lowerBound;

                    float delta = static_cast<float>(rtsParams.delta);
                    if (ImGui::SliderFloat("Delta", &delta, 0.01f,
                                           0.25f, "%.2f"))
                        rtsParams.delta = delta;

                    float epsilon =
                        static_cast<float>(rtsParams.epsilon);
                    if (ImGui::SliderFloat("Epsilon", &epsilon, 0.01f,
                                           0.25f, "%.2f"))
                        rtsParams.epsilon = epsilon;

                    ImGui::Spacing();
                    ImGui::Spacing();

                    bool running = channel.is_running.load();

                    if (!running)
                    {
                        if (ImGui::Button("Run")) startWorker();
                    }
                    else
                    {
                        if (ImGui::Button("Cancel##worker"))
                            cancelWorker();
                        ImGui::SameLine();
                        float p = channel.progress.load();
                        ImGui::ProgressBar(
                            p, ImVec2(ImGui::GetContentRegionAvail().x,
                                      0.0f));
                    }

                    if (statusText[0] != '\0')
                    {
                        if (statusIsError)
                        {
                            ImGui::PushStyleColor(
                                ImGuiCol_Text,
                                ImVec4(1, 0.2f, 0.2f, 1));
                            ImGui::TextWrapped("%s", statusText);
                            ImGui::PopStyleColor();
                        }
                        else { ImGui::TextWrapped("%s", statusText); }
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            // Drain messages outside of tabs so
            // they are processed regardless of
            // which tab is active.
            while (auto msg = channel.messages.pop())
            {
                if (msg->kind == MsgKind::Done)
                {
                    workerFinished = true;
                    cleanup();
                }
                else if (msg->kind == MsgKind::Cancelled)
                {
                    workerFinished = false;
                    cleanup();
                }
                if (msg->kind == MsgKind::Error ||
                    msg->kind == MsgKind::Progress ||
                    msg->kind == MsgKind::Done)
                {
                    std::strncpy(statusText, msg->text,
                                 sizeof(statusText) - 1);
                    statusText[sizeof(statusText) - 1] = '\0';
                    statusIsError = (msg->kind == MsgKind::Error);
                }
            }

            ImGui::Separator();

            if (ImGui::Button("Exit"))
            {
                cancelWorker();
                open = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        return dialogResult;
    }

    void AlignDialog::startWorker()
    {
        cleanup();
        channel.cancel_requested.store(false);
        channel.is_running.store(true);
        channel.progress.store(0.0f);
        workerFinished = false;
        statusText[0] = '\0';
        statusIsError = false;

        auto left = leftImage;
        auto right = rightImage;
        auto params = rtsParams;
        workerThread = std::thread(
            [this, left, right, params]()
            {
                runAutoAlign(*left, *right, channel, workerResults,
                             params);
            });
    }

    void AlignDialog::cancelWorker()
    {
        channel.cancel_requested.store(true);
    }

    void AlignDialog::cleanup()
    {
        if (workerThread.joinable())
        {
            workerThread.join();
        }
    }

}  // namespace shoecomp
