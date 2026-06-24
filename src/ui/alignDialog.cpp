#include "ui/alignDialog.h"
#include "ui/imageCanvas2d.h"
#include "ui/uiHelpers.h"
#include "calc/align.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace shoecomp
{
    ImVec2 AlignState::transformRight2Left(const ImVec2& pt) const
    {
        /* pt is in image coordinates */
        ImVec2 result;
        float cos_theta = cosf(this->rotation) * this->scale;
        float sin_theta = sinf(this->rotation) * this->scale;
        result.x = pt.x * cos_theta - pt.y * sin_theta;
        result.y = pt.x * sin_theta + pt.y * cos_theta;
        result.x += this->dx;
        result.y += this->dy;
        return result;
    }

    ImVec2 AlignState::transformLeft2Right(const ImVec2& pt) const
    {
        /* pt is in image coordinates */
        ImVec2 result;
        float cos_theta = cosf(-this->rotation) / this->scale;
        float sin_theta = sinf(-this->rotation) / this->scale;
        float tempX = pt.x - this->dx;
        float tempY = pt.y - this->dy;
        result.x = tempX * cos_theta - tempY * sin_theta;
        result.y = tempX * sin_theta + tempY * cos_theta;
        return result;
    }

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

    static int countPts(
        const std::shared_ptr<ImageCanvas2D::ImageData>& img)
    {
        if (!img) return 0;
        if (!hasAnnotationArray(img->annotations, "points")) return 0;
        return static_cast<int>(
            img->annotations["points"].getArray().size());
    };

    void AlignDialog::render()
    {
        static int maxPts = -1;
        if (show)
        {
            show = false;
            open = true;
            ImGui::OpenPopup("Align Images");
        }

        // Always drain so workerFinished is up to
        // date even after the popup closes.
        drainMessages();

        if (!open)
        {
            maxPts = -1;
            return;
        }
        if (maxPts == -1)
        {
            maxPts = 256;
            int lp = countPts(leftImage);
            int rp = countPts(rightImage);
            maxPts = std::max(3, std::min(lp, rp));
            rtsParams.upperBound = maxPts;
        }

        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(ds.x * 0.5f, ds.y * 0.7f),
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

            float labelW = ImGui::CalcTextSize("Bounds").x +
                           ImGui::GetStyle().ItemInnerSpacing.x;
            float widgetW = ImGui::CalcItemWidth() - labelW -
                            ImGui::GetStyle().ItemSpacing.x;
            float halfW = widgetW * 0.5f;
            ImGui::SetNextItemWidth(halfW);
            ImGui::SliderInt("##Lower", &rtsParams.lowerBound, 3,
                             maxPts, "Min: %d");
            if (rtsParams.lowerBound < 3) rtsParams.lowerBound = 3;
            if (rtsParams.upperBound < rtsParams.lowerBound)
                rtsParams.upperBound = rtsParams.lowerBound;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(halfW);
            ImGui::SliderInt("Bounds", &rtsParams.upperBound,
                             rtsParams.lowerBound, maxPts, "Max: %d");

            if (!rtsParams.sameScale)
            {
                float scaleRange =
                    static_cast<float>(rtsParams.scaleRange);
                if (ImGui::SliderFloat("Scale Range", &scaleRange, 1.0f,
                                       10.0f, "%.2f"))
                    rtsParams.scaleRange = scaleRange;
            }

            float epsilon = static_cast<float>(rtsParams.epsilon);
            if (ImGui::SliderFloat("Allowed Error", &epsilon, 0.001f,
                                   0.25f, "%.3f"))
                rtsParams.epsilon = epsilon;

            ImGui::Checkbox("Same Scale", &rtsParams.sameScale);

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
                else
                {
                    ImGui::TextWrapped("%s", statusText);
                }
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
