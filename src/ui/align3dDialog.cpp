#include "ui/align3dDialog.h"

#include <cstdio>
#include <cstring>

#include "imgui.h"

namespace shoecomp
{
    bool Align3DDialog::openFor(const ImageCanvas3D& left,
                                const ImageCanvas3D& right)
    {
        cancel();
        cleanup();
        statusText[0] = '\0';
        statusIsError = false;
        if (!left.surface || !right.surface)
        {
            std::strncpy(statusText, "no surface loaded",
                         sizeof(statusText) - 1);
            statusIsError = true;
            return false;
        }
        std::vector<StriationLine> l =
            surfaceStriations(*left.surface, &leftPeakIdx);
        std::vector<StriationLine> r =
            surfaceStriations(*right.surface, &rightPeakIdx);
        if ((int)l.size() < settings.minLines ||
            (int)r.size() < settings.minLines)
        {
            std::snprintf(statusText, sizeof(statusText),
                          "Need at least %d striations on each land "
                          "(found %zu and %zu). Run Detect first.",
                          settings.minLines, l.size(), r.size());
            statusIsError = true;
            return false;
        }
        leftRows = striationRows(l);
        rightRows = striationRows(r);
        leftName = left.name();
        rightName = right.name();
        results.clear();
        workerFinished = false;
        show = true;
        return true;
    }

    void Align3DDialog::drainMessages()
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
                if (msg->text[0] == '\0') continue;
                std::strncpy(statusText, msg->text,
                             sizeof(statusText) - 1);
                statusText[sizeof(statusText) - 1] = '\0';
                statusIsError = (msg->kind == MsgKind::Error);
            }
        }
        if (!channel.is_running.load() && workerThread.joinable())
        {
            cleanup();
            workerFinished = !results.empty();
        }
    }

    void Align3DDialog::render()
    {
        if (show)
        {
            show = false;
            open = true;
            ImGui::OpenPopup("Align Surfaces");
        }
        drainMessages();
        if (!open) return;

        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(ds.x * 0.5f, ds.y * 0.55f),
                                 ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.25f, ds.y * 0.2f),
                                ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal(
                "Align Surfaces", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
            return;

        ImGui::SetWindowFontScale(0.9f);
        ImGui::TextWrapped(
            "Aligning %s to %s from %d and %d striations.",
            rightName.c_str(), leftName.c_str(), (int)rightRows.rows(),
            (int)leftRows.rows());
        ImGui::Separator();

        const bool running = channel.is_running.load();
        ImGui::BeginDisabled(running);
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
        ImGui::DragFloat("Angle tolerance (deg)", &settings.angleTolDeg,
                         0.05f, 0.01f, 10.0f, "%.2f");
        ImGui::DragFloat("Separation tolerance (um)",
                         &settings.sepTolUm, 0.1f, 0.1f, 100.0f,
                         "%.1f");
        ImGui::DragFloat("Max length ratio (0 = off)",
                         &settings.maxLengthRatio, 0.05f, 0.0f, 10.0f,
                         "%.2f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Only match striations whose lengths "
                "differ by at most this factor.");
        ImGui::DragFloat("Extent allowance (0 = off)",
                         &settings.extentScale, 0.05f, 0.0f, 5.0f,
                         "%.2f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Widen the separation tolerance for striations "
                "detected over different lengths.");
        ImGui::InputInt("Min matched lines", &settings.minLines);
        if (settings.minLines < 2) settings.minLines = 2;
        ImGui::InputInt("Results", &settings.numResults);
        if (settings.numResults < 1) settings.numResults = 1;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Distinct alignments, best first: each "
                "later one is a different transform.");
        ImGui::DragFloat("Later results: min fraction of best",
                         &settings.distinctMinFrac, 0.01f, 0.0f, 1.0f,
                         "%.2f");
        ImGui::DragFloat("Max rotation (deg, 0 = off)",
                         &settings.maxRotationDeg, 1.0f, 0.0f, 180.0f,
                         "%.0f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Reject alignments that rotate one land by more "
                "than this,\ne.g. reversed end-for-end.");
        ImGui::SliderInt("Workers", &numWorkers, 1, 16);
        ImGui::PopItemWidth();
        ImGui::EndDisabled();

        ImGui::Spacing();
        if (!running)
        {
            if (ImGui::Button("Run")) start();
        }
        else
        {
            if (ImGui::Button("Cancel##align3d")) cancel();
            ImGui::SameLine();
            ImGui::ProgressBar(
                channel.progress.load(),
                ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
        }

        if (statusText[0] != '\0')
        {
            if (statusIsError)
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(1, 0.2f, 0.2f, 1));
            ImGui::TextWrapped("%s", statusText);
            if (statusIsError) ImGui::PopStyleColor();
        }

        ImGui::Separator();
        if (ImGui::Button("Close"))
        {
            if (running) cancel();
            cleanup();
            drainMessages();
            open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void Align3DDialog::start()
    {
        cleanup();
        channel.cancel_requested.store(false);
        channel.is_running.store(true);
        channel.progress.store(0.0f);
        workerFinished = false;
        results.clear();
        statusText[0] = '\0';
        statusIsError = false;

        StriationAlignParams p = settings.toParams((size_t)numWorkers);
        workerThread = std::thread(
            [this, p]()
            {
                bool ok = runStriationAlign(leftRows, rightRows, p,
                                            channel, results);
                if (channel.should_cancel() || !ok)
                {
                    results.clear();
                    channel.cancelled();
                    return;
                }
                char buf[96];
                std::snprintf(buf, sizeof(buf),
                              "%zu alignment(s), best matches %zu "
                              "striations",
                              results.size(),
                              results.front().pairs.size());
                channel.report(1.0f, buf);
                channel.done();
            });
    }

    void Align3DDialog::cancel()
    {
        channel.cancel_requested.store(true);
    }

    void Align3DDialog::cleanup()
    {
        if (workerThread.joinable()) workerThread.join();
    }
}  // namespace shoecomp
