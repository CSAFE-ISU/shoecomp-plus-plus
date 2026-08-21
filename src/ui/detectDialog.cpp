#include "ui/detectDialog.h"

#include "formats/png.h"
#include "ui/imageCanvas2d.h"
#include "ui/loadBrowser.h"
#include "ui/uiHelpers.h"
#include "imgui.h"
#include "jtjson/json.h"

#include <cstdio>
#include <cstring>

namespace shoecomp
{
    bool DetectDialog::openFor(ImageCanvas2D& canvas)
    {
        if (!canvas.hasImage() || !canvas.image) return false;

        // Make sure no prior worker is still reading `frame` before we
        // overwrite it.
        cancelInference();
        cleanup();

        targetImage = canvas.image;
        spec = canvas.detectionSpec();
        confThreshold = spec.confThreshold;
        iouThreshold = spec.iouThreshold;

        frameW = targetImage->width;
        frameH = targetImage->height;
        frame.assign((size_t)frameW * frameH * 4, 0);
        if (!saveTextureRGBA(targetImage->textureId, frameW, frameH,
                             frame.data()))
        {
            std::strncpy(statusText,
                         "Could not read image pixels for detection.",
                         sizeof(statusText) - 1);
            statusIsError = true;
            targetImage.reset();
            frame.clear();
            return false;
        }

        results.clear();
        appliedCount = -1;
        workerFinished = false;
        statusText[0] = '\0';
        statusIsError = false;

        weightsBrowser.extension = ".onnx";
        weightsBrowser.extensionChoices = {".onnx"};
        weightsBrowser.loadCorrespondingJson = false;
        weightsBrowser.dirNeedsRefresh = true;
        weightsBrowser.onSelect =
            [this](const std::string& path, const std::string&)
        { modelPath = path; };

        show = true;
        return true;
    }

    void DetectDialog::applyResults()
    {
        appliedCount = 0;
        if (!targetImage) return;

        jt::Json& ann = targetImage->annotations;
        if (!hasAnnotationArray(ann, "points"))
        {
            if (!ann.isObject()) ann.setObject();
            ann["points"].setArray();
        }
        auto& pts = ann["points"].getArray();

        const int nTypes = (int)spec.classToPointType.size();
        for (const auto& d : results)
        {
            if (d.cls < 0 || d.cls >= nTypes) continue;  // unmapped
            jt::Json pt;
            pt.setObject();
            pt["x"] = d.cx;
            pt["y"] = d.cy;
            pt["type"] = ImageCanvas2D::pointTypeToString(
                spec.classToPointType[d.cls]);
            pts.push_back(std::move(pt));
            ++appliedCount;
        }

        std::snprintf(statusText, sizeof(statusText),
                      "%d of %zu detection(s) added as points.",
                      appliedCount, results.size());
        statusIsError = false;
    }

    void DetectDialog::drainMessages()
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

    void DetectDialog::render()
    {
        if (show)
        {
            show = false;
            open = true;
            ImGui::OpenPopup("Detect Points");
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

            if (ImGui::BeginPopupModal("Detect Points", nullptr,
                                       ImGuiWindowFlags_NoResize |
                                           ImGuiWindowFlags_NoMove))
            {
                ImGui::Text("Automatic point detection (%dx%d image)",
                            frameW, frameH);
                ImGui::TextWrapped(
                    "Runs a YOLO-style ONNX model; each detected box "
                    "center is added as an annotation point.");
                ImGui::Separator();

                ImGui::TextUnformatted("Model:");
                ImGui::SameLine();
                ImGui::TextWrapped("%s", modelPath.empty()
                                             ? "(none selected)"
                                             : modelPath.c_str());
                bool running = channel.is_running.load();
                ImGui::BeginDisabled(running);
                if (ImGui::Button("Browse..."))
                    openImagePicker(weightsBrowser);
                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Text("Input: %dx%d   Classes: %zu",
                            spec.inputWidth, spec.inputHeight,
                            spec.classToPointType.size());
                ImGui::BeginDisabled(running);
                ImGui::SliderFloat("Confidence", &confThreshold, 0.0f,
                                   1.0f, "%.2f");
                ImGui::SliderFloat("IoU (NMS)", &iouThreshold, 0.0f,
                                   1.0f, "%.2f");
                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Spacing();

                if (!running)
                {
                    ImGui::BeginDisabled(modelPath.empty());
                    if (ImGui::Button("Run")) startInference();
                    ImGui::EndDisabled();
                }
                else
                {
                    if (ImGui::Button("Cancel##detect"))
                        cancelInference();
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
                    if (running) cancelInference();
                    drainMessages();
                    open = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }

        // Render the model picker at the top popup level (it stacks
        // over the detect modal when opened).
        weightsBrowser.render();
    }

    void DetectDialog::startInference()
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

        // Snapshot the run configuration; the worker reads members that
        // the UI does not mutate while a run is in flight.
        std::string model = modelPath;
        float conf = confThreshold;
        float iou = iouThreshold;
        workerThread = std::thread(
            [this, model, conf, iou]()
            {
                DetectionInput in;
                in.rgba = frame.data();
                in.imgW = frameW;
                in.imgH = frameH;
                in.inputW = spec.inputWidth;
                in.inputH = spec.inputHeight;
                in.nchw = spec.nchw;
                in.normalize = spec.normalize;
                in.confThreshold = conf;
                in.iouThreshold = iou;

                channel.report(0.1f, "running inference...");
                std::string err;
                bool ok = runYoloDetection(model, in, results, err,
                                           &channel.cancel_requested);
                if (channel.should_cancel())
                {
                    channel.cancelled();
                    return;
                }
                if (!ok)
                {
                    channel.error(err.empty() ? "inference failed"
                                              : err.c_str());
                    channel.cancelled();
                    return;
                }
                char buf[96];
                std::snprintf(buf, sizeof(buf),
                              "%zu detection(s) found", results.size());
                channel.report(1.0f, buf);
                channel.done();
            });
    }

    void DetectDialog::cancelInference()
    {
        channel.cancel_requested.store(true);
    }

    void DetectDialog::cleanup()
    {
        if (workerThread.joinable()) workerThread.join();
    }
}  // namespace shoecomp
