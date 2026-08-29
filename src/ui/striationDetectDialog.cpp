#include "ui/striationDetectDialog.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "formats/x3pAnnotationIo.h"
#include "imgui.h"
#include "jtjson/json.h"

namespace shoecomp
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;
    }

    bool StriationDetectDialog::openFor(X3PBulletLand& canvas)
    {
        if (!canvas.surface || !canvas.hasFit || !canvas.fit.valid)
        {
            std::strncpy(statusText,
                         "cannot detect without a cylinder fit",
                         sizeof(statusText) - 1);
            statusIsError = true;
            return false;
        }

        // Make sure no prior worker is still reading the surface/fit.
        cancelDetection();
        cleanup();

        surface = canvas.surface;
        fit = canvas.fit;

        results.clear();
        appliedCount = -1;
        workerFinished = false;
        statusText[0] = '\0';
        statusIsError = false;

        show = true;
        return true;
    }

    void StriationDetectDialog::applyResults()
    {
        appliedCount = 0;
        if (!surface) return;

        jt::Json& ann = surface->annotations;
        if (!ann.isObject()) ann.setObject();
        ann["peaks"].setArray();  // replace any existing peaks
        auto& arr = ann["peaks"].getArray();

        for (const auto& d : results) arr.push_back(peak5DToJson(d));
        appliedCount = (int)results.size();

        std::snprintf(statusText, sizeof(statusText),
                      "%d striation(s) detected.", appliedCount);
        statusIsError = false;
    }

    void StriationDetectDialog::drainMessages()
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

        // Recover if the worker stopped without a terminal message.
        if (!channel.is_running.load() && workerThread.joinable())
        {
            cleanup();
            workerFinished = !results.empty();
        }

        // Apply detections exactly once, after the worker has joined.
        if (workerFinished && appliedCount < 0 &&
            !workerThread.joinable())
        {
            applyResults();
        }
    }

    void StriationDetectDialog::render()
    {
        if (show)
        {
            show = false;
            open = true;
            ImGui::OpenPopup("Detect Striations");
        }

        // Drain every frame so status/results stay current even after
        // the popup is dismissed.
        drainMessages();

        if (open)
        {
            ImVec2 ds = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowSize(ImVec2(ds.x * 0.5f, ds.y * 0.5f),
                                     ImGuiCond_Appearing);
            ImGui::SetNextWindowPos(ImVec2(ds.x * 0.25f, ds.y * 0.25f),
                                    ImGuiCond_Appearing);

            if (ImGui::BeginPopupModal("Detect Striations", nullptr,
                                       ImGuiWindowFlags_NoResize |
                                           ImGuiWindowFlags_NoMove))
            {
                ImGui::SetWindowFontScale(0.9f);
                ImGui::TextWrapped(
                    "Detect axial ridges and grooves on the fitted "
                    "cylinder. Detected striations REPLACE the current "
                    "peaks.");
                ImGui::Separator();

                bool running = channel.is_running.load();
                ImGui::BeginDisabled(running);
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x *
                                     0.4f);
                ImGui::DragFloat("Min angular width (deg)",
                                 &minThetaDeg, 0.05f, 0.0f, 45.0f,
                                 "%.2f");
                ImGui::DragFloat("Min height |dr| (um)", &minDrUm, 0.1f,
                                 0.0f, 1000.0f, "%.2f");
                ImGui::DragFloat("Min axial length (mm)", &minLengthMm,
                                 0.01f, 0.0f, 100.0f, "%.3f");
                ImGui::InputInt("Max count", &maxCount);
                if (maxCount < 1) maxCount = 1;
                ImGui::PopItemWidth();
                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Spacing();

                if (!running)
                {
                    if (ImGui::Button("Run")) startDetection();
                }
                else
                {
                    if (ImGui::Button("Cancel##striation"))
                        cancelDetection();
                    ImGui::SameLine();
                    ImGui::ProgressBar(
                        channel.progress.load(),
                        ImVec2(ImGui::GetContentRegionAvail().x, 0.0f));
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
                if (ImGui::Button("Close"))
                {
                    if (running) cancelDetection();
                    drainMessages();
                    open = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }
    }

    void StriationDetectDialog::startDetection()
    {
        cleanup();
        channel.cancel_requested.store(false);
        channel.is_running.store(true);
        channel.progress.store(0.0f);
        workerFinished = false;
        appliedCount = -1;
        results.clear();
        statusText[0] = '\0';
        statusIsError = false;

        StriationParams params;
        params.minThetaDiff = minThetaDeg * kPi / 180.0;
        params.minDr = minDrUm * 1e-6;
        params.minLength = minLengthMm * 1e-3;
        params.maxCount = maxCount;

        // The worker holds its own shared_ptr + fit copy so the run
        // stays valid even if the dialog target changes.
        std::shared_ptr<Surface3D> surf = surface;
        CylinderFit f = fit;
        workerThread = std::thread(
            [this, surf, f, params]()
            {
                std::string err;
                bool ok =
                    detectStriations(f, *surf, params, results, err,
                                     &channel.cancel_requested,
                                     [this](float p, const char* t)
                                     { channel.report(p, t); });
                if (channel.should_cancel())
                {
                    channel.cancelled();
                    return;
                }
                if (!ok)
                {
                    channel.error(err.empty() ? "detection failed"
                                              : err.c_str());
                    channel.cancelled();
                    return;
                }
                char buf[96];
                std::snprintf(buf, sizeof(buf),
                              "%zu striation(s) found", results.size());
                channel.report(1.0f, buf);
                channel.done();
            });
    }

    void StriationDetectDialog::cancelDetection()
    {
        channel.cancel_requested.store(true);
    }

    void StriationDetectDialog::cleanup()
    {
        if (workerThread.joinable()) workerThread.join();
    }
}  // namespace shoecomp
