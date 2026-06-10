#include "ui/mainWindow.h"
#include "ui/imageCanvas.h"
#include "ui/uiHelpers.h"
#include <algorithm>
#include <cmath>

namespace shoecomp
{
    void renderLockToggle(bool& locked)
    {
        if (ImGui::Button(locked ? "Unlock" : "Lock")) locked = !locked;
    }

    void ImageCanvas::renderViewerPanel(
        std::vector<ImageCanvas>& images, int& selectedIdx,
        int otherIdx, ImageCanvas& viewer, bool locked,
        const char* label)
    {
        const char* preview =
            (selectedIdx >= 0 && selectedIdx < (int)images.size())
                ? images[selectedIdx].image->name.c_str()
                : "<none>";

        ImGui::BeginDisabled(locked);
        if (ImGui::BeginCombo(label, preview))
        {
            if (ImGui::Selectable("<none>", selectedIdx < 0))
            {
                selectedIdx = -1;
                viewer.viewState.zoom = viewer.viewState.zoomTarget =
                    1.0f;
                viewer.viewState.pan = viewer.viewState.panTarget =
                    ImVec2(0, 0);
            }
            for (int i = 0; i < (int)images.size(); ++i)
            {
                if (i == otherIdx) continue;
                bool selected = (i == selectedIdx);
                if (ImGui::Selectable(images[i].image->name.c_str(),
                                      selected))
                {
                    selectedIdx = i;
                    viewer.viewState.zoom =
                        viewer.viewState.zoomTarget = 0.0f;
                    viewer.viewState.pan = viewer.viewState.panTarget =
                        ImVec2(0, 0);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        if (selectedIdx < 0 || selectedIdx >= (int)images.size())
            return;

        viewer.image = images[selectedIdx].image;
        float toolbarH = ImGui::GetFrameHeightWithSpacing() * 3.0f;
        ImVec2 region = ImGui::GetContentRegionAvail();
        float canvasH = region.y - toolbarH;
        if (canvasH > 0.0f)
        {
            ImGui::BeginChild("##cvs", ImVec2(0, canvasH),
                              ImGuiChildFlags_None);
            viewer.renderCanvas("##canvas");
            ImGui::EndChild();
        }
        viewer.renderToolbar(label);
    }

    void renderImageComparison(AppState& state)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float dockH = ImGui::GetFrameHeightWithSpacing() +
                      ImGui::GetStyle().ItemSpacing.y;
        float contentH = avail.y - dockH;

        ImGui::BeginChild("ComparisonContent",
                          ImVec2(avail.x, contentH),
                          ImGuiChildFlags_None);
        {
            float totalW = ImGui::GetContentRegionAvail().x;
            float splitterW = 8.0f;
            float leftW =
                totalW * state.viewerSplitRatio - splitterW * 0.5f;
            float rightW = totalW * (1.0f - state.viewerSplitRatio) -
                           splitterW * 0.5f;

            // Snapshot targets before rendering so
            // we can detect which viewer changed.
            ImageCanvas::ViewTargets l0 =
                ImageCanvas::snapshotTargets(state.viewerLeft);
            ImageCanvas::ViewTargets r0 =
                ImageCanvas::snapshotTargets(state.viewerRight);

            ImGui::BeginChild("LeftViewer", ImVec2(leftW, 0),
                              ImGuiChildFlags_Borders);
            ImageCanvas::renderViewerPanel(
                state.images, state.viewerLeftIdx, state.viewerRightIdx,
                state.viewerLeft, state.viewerLocked, "##Left");
            ImGui::EndChild();

            ImGui::SameLine();

            // Draggable splitter
            float height = ImGui::GetContentRegionAvail().y;
            ImGui::Button("##Splitter", ImVec2(splitterW, height));
            if (ImGui::IsItemActive())
            {
                float delta = ImGui::GetIO().MouseDelta.x;
                state.viewerSplitRatio += delta / totalW;
                state.viewerSplitRatio =
                    std::clamp(state.viewerSplitRatio, 0.1f, 0.9f);
            }
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

            ImGui::SameLine();

            ImGui::BeginChild("RightViewer", ImVec2(rightW, 0),
                              ImGuiChildFlags_Borders);
            ImageCanvas::renderViewerPanel(
                state.images, state.viewerRightIdx, state.viewerLeftIdx,
                state.viewerRight, state.viewerLocked, "##Right");
            ImGui::EndChild();

            // Apply locked sync with alignment
            // offset after both viewers render.
            if (state.viewerLocked && !state.alignEditPopupVisible)
            {
                ImageCanvas::syncPair(
                    state.viewerLeft, state.viewerRight, l0, r0,
                    state.viewerAlignments[state.viewerAlignmentIdx],
                    state.viewerLocked, state.alignDialog.open,
                    state.settings);
            }
            else
            {
                state.viewerLeft.viewState.homeRequested = false;
                state.viewerRight.viewState.homeRequested = false;
            }
        }
        ImGui::EndChild();

        // --- Dock bar ---
        ImGui::Separator();
        bool hasLeft = state.viewerLeftIdx >= 0 &&
                       state.viewerLeftIdx < (int)state.images.size();
        bool hasRight = state.viewerRightIdx >= 0 &&
                        state.viewerRightIdx < (int)state.images.size();
        bool hasBoth = hasLeft && hasRight;
        ImGui::BeginDisabled(!hasBoth);
        if (ImGui::Button(state.viewerLocked ? "Unlock" : "Lock"))
            state.viewerLocked = !state.viewerLocked;
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!hasBoth);
        if (ImGui::Button("Align"))
        {
            state.alignDialog.show = true;
            state.alignDialog.leftName =
                state.images[state.viewerLeftIdx].image->name;
            state.alignDialog.rightName =
                state.images[state.viewerRightIdx].image->name;
            state.alignDialog.leftImage =
                state.images[state.viewerLeftIdx].image;
            state.alignDialog.rightImage =
                state.images[state.viewerRightIdx].image;
            state.viewerLocked = false;
            state.viewerLeft.resetView();
            state.viewerRight.resetView();
            state.viewerLocked = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!state.viewerLocked);
        if (ImGui::Button("Save Alignment"))
        {
            state.alignmentSaveBrowser.show = true;
            state.alignmentSaveBrowser.dirNeedsRefresh = true;
            state.alignmentSaveBrowser.fileName.clear();
        }
        ImGui::EndDisabled();

        if (state.viewerLocked)
        {
            ImGui::SameLine();
            bool navDisabled = (int)state.viewerAlignments.size() <= 1;
            if (navDisabled) ImGui::BeginDisabled();
            if (ImGui::Button("<"))
            {
                if (state.viewerAlignmentIdx > 0)
                {
                    state.viewerAlignmentIdx--;
                    ImageCanvas::applyAlignment(
                        state.viewerLeft, state.viewerRight,
                        state
                            .viewerAlignments[state.viewerAlignmentIdx],
                        state.viewerLocked);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(">"))
            {
                int maxIdx = (int)state.viewerAlignments.size() - 1;
                if (state.viewerAlignmentIdx < maxIdx)
                {
                    state.viewerAlignmentIdx++;
                    ImageCanvas::applyAlignment(
                        state.viewerLeft, state.viewerRight,
                        state
                            .viewerAlignments[state.viewerAlignmentIdx],
                        state.viewerLocked);
                }
            }
            if (navDisabled) ImGui::EndDisabled();

            auto& a = state.viewerAlignments[state.viewerAlignmentIdx];
            ImGui::SameLine();
            ImGui::Text(
                "[%d/%d] [%s] R:%.1f"
                " T:(%.0f,%.0f) S:%.2f",
                state.viewerAlignmentIdx + 1,
                (int)state.viewerAlignments.size(),
                a.mode == AlignMode::Manual ? "Manual" : "Auto",
                a.rotation / kDegToRad, a.dx, a.dy, a.scale);

            ImGui::SameLine();
            bool delDisabled = (int)state.viewerAlignments.size() <= 1;
            if (delDisabled) ImGui::BeginDisabled();
            if (ImGui::Button("Delete"))
            {
                int idx = state.viewerAlignmentIdx;
                state.viewerAlignments.erase(
                    state.viewerAlignments.begin() + idx);
                if (idx >= (int)state.viewerAlignments.size())
                    idx = (int)state.viewerAlignments.size() - 1;
                state.viewerAlignmentIdx = idx;
                ImageCanvas::applyAlignment(
                    state.viewerLeft, state.viewerRight,
                    state.viewerAlignments[idx], state.viewerLocked);
            }
            if (delDisabled) ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Edit"))
            {
                state.alignEditState =
                    state.viewerAlignments[state.viewerAlignmentIdx];
                state.alignEditOriginal = state.alignEditState;
                state.alignEditOpen = true;
            }
        }
    }

}  // namespace shoecomp
