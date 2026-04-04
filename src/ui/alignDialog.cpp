#include "ui/alignDialog.h"
#include "ui/imageCanvas.h"
#include "ui/uiHelpers.h"
#include "calc/align.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace shoecomp
{
    void AlignDialog::drainMessages()
    {
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
                if (!msg->text || msg->text[0] == '\0') continue;
                std::strncpy(statusText, msg->text,
                             sizeof(statusText) - 1);
                statusText[sizeof(statusText) - 1] = '\0';
                statusIsError = (msg->kind == MsgKind::Error);
            }
        }

        // The SPSC ring buffer silently drops
        // messages when full. If the worker
        // stopped but we never saw Done or
        // Cancelled, recover here.
        if (!channel.is_running.load() && workerThread.joinable())
        {
            cleanup();
            workerFinished = !workerResults.empty();
        }
    }

    void AlignDialog::render()
    {
        if (show)
        {
            show = false;
            open = true;
            ImGui::OpenPopup("Align Images");
        }

        // Always drain so workerFinished is up to
        // date even after the popup closes.
        drainMessages();

        if (!open) return;

        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(ds.x * 0.5f, ds.y * 0.6f),
                                 ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.25f, ds.y * 0.2f),
                                ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal(
                "Align Images", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("Aligning %s to %s", rightName.c_str(),
                        leftName.c_str());
            ImGui::Separator();

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

            int maxPts = 256;
            auto countPts =
                [](const std::shared_ptr<ImageData>& img) -> int
            {
                if (!img) return 0;
                if (!hasAnnotationArray(img->annotations, "points"))
                    return 0;
                return static_cast<int>(
                    img->annotations["points"].getArray().size());
            };
            int lp = countPts(leftImage);
            int rp = countPts(rightImage);
            maxPts = std::max(3, std::min(lp, rp));
            rtsParams.upperBound = maxPts;

            ImGui::SliderInt("Lower Bound", &rtsParams.lowerBound, 3,
                             maxPts);
            if (rtsParams.lowerBound < 3) rtsParams.lowerBound = 3;

            ImGui::SliderInt("Upper Bound", &rtsParams.upperBound,
                             rtsParams.lowerBound, maxPts);
            if (rtsParams.upperBound < rtsParams.lowerBound)
                rtsParams.upperBound = rtsParams.lowerBound;

            float delta = static_cast<float>(rtsParams.delta);
            if (ImGui::SliderFloat("Delta", &delta, 0.01f, 0.25f,
                                   "%.2f"))
                rtsParams.delta = delta;

            float epsilon = static_cast<float>(rtsParams.epsilon);
            if (ImGui::SliderFloat("Epsilon", &epsilon, 0.01f, 0.25f,
                                   "%.2f"))
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
                if (ImGui::Button("Cancel##worker")) cancelWorker();
                ImGui::SameLine();
                float p = channel.progress.load();
                ImGui::ProgressBar(
                    p, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
            }

            if (statusText[0] != '\0')
            {
                if (statusIsError)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(1, 0.2f, 0.2f, 1));
                    ImGui::TextWrapped("%s", statusText);
                    ImGui::PopStyleColor();
                }
                else { ImGui::TextWrapped("%s", statusText); }
            }

            ImGui::Separator();

            if (ImGui::Button("Exit"))
            {
                if (!workerFinished) cancelWorker();
                drainMessages();
                open = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
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
        if (workerThread.joinable()) { workerThread.join(); }
    }

}  // namespace shoecomp
