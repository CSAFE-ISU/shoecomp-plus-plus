#include "ui/mainWindow.h"
#include "ui/imageCanvas2d.h"
#include "ui/uiHelpers.h"
#include <algorithm>
#include <cmath>

namespace shoecomp
{
    void renderLockToggle(bool& locked)
    {
        if (ImGui::Button(locked ? "Unlock" : "Lock")) locked = !locked;
    }

    void ImageCanvas2D::renderViewerPanel(
        std::vector<std::unique_ptr<ImageCanvas>>& images,
        int& selectedIdx, int otherIdx, bool locked, const char* label)
    {
        const char* preview =
            (selectedIdx >= 0 && selectedIdx < (int)images.size())
                ? images[selectedIdx]->name().c_str()
                : "<none>";

        ImGui::BeginDisabled(locked);
        if (ImGui::BeginCombo(label, preview))
        {
            if (ImGui::Selectable("<none>", selectedIdx < 0))
            {
                selectedIdx = -1;
                viewState.zoom = viewState.zoomTarget = 1.0f;
                viewState.pan = viewState.panTarget = ImVec2(0, 0);
            }
            for (int i = 0; i < (int)images.size(); ++i)
            {
                if (i == otherIdx) continue;
                bool selected = (i == selectedIdx);
                if (ImGui::Selectable(images[i]->name().c_str(),
                                      selected))
                {
                    selectedIdx = i;
                    viewState.zoom = viewState.zoomTarget = 0.0f;
                    viewState.pan = viewState.panTarget = ImVec2(0, 0);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        if (selectedIdx < 0 || selectedIdx >= (int)images.size())
            return;

        ImageCanvas2D* sel = asCanvas2D(*images[selectedIdx]);
        if (!sel) return;
        image = sel->image;
        float toolbarH = ImGui::GetFrameHeightWithSpacing() * 1.6f;
        ImVec2 region = ImGui::GetContentRegionAvail();
        float canvasH = region.y - toolbarH;
        if (canvasH > 0.0f)
        {
            ImGui::BeginChild("##cvs", ImVec2(0, canvasH),
                              ImGuiChildFlags_None);
            renderCanvas("##canvas");
            ImGui::EndChild();
        }
        renderToolbar(label);
    }

    static void renderComparisonContent(AppState& state, float contentW,
                                        float contentH)
    {
        ImGui::BeginChild("ComparisonContent",
                          ImVec2(contentW, contentH),
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
                state.viewerLeft->snapshotTargets();
            ImageCanvas::ViewTargets r0 =
                state.viewerRight->snapshotTargets();

            ImGui::BeginChild("LeftViewer", ImVec2(leftW, 0),
                              ImGuiChildFlags_Borders);
            state.viewerLeft->renderViewerPanel(
                state.images, state.viewerLeftIdx, state.viewerRightIdx,
                state.viewerLocked, "##Left");
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
            state.viewerRight->renderViewerPanel(
                state.images, state.viewerRightIdx, state.viewerLeftIdx,
                state.viewerLocked, "##Right");
            ImGui::EndChild();

            // Track which viewer is active for markup edits.
            ImageCanvas2D* lc = asCanvas2D(*state.viewerLeft);
            ImageCanvas2D* rc = asCanvas2D(*state.viewerRight);
            if (lc && lc->viewState.isHovered)
                state.activeComparisonViewer = 0;
            else if (rc && rc->viewState.isHovered)
                state.activeComparisonViewer = 1;

            // Apply locked sync with alignment
            // offset after both viewers render.
            if (state.viewerLocked && !state.alignEditPopupVisible)
            {
                state.viewerLeft->syncPair(
                    *state.viewerRight, l0, r0,
                    state.viewerAlignments[state.viewerAlignmentIdx],
                    state.viewerLocked, state.alignDialog.open,
                    state.settings);
            }
            else
            {
                state.viewerLeft->clearHomeRequested();
                state.viewerRight->clearHomeRequested();
            }
        }
        ImGui::EndChild();
    }

    static void renderComparisonDock(AppState& state)
    {
        bool hasLeft = state.viewerLeftIdx >= 0 &&
                       state.viewerLeftIdx < (int)state.images.size();
        bool hasRight = state.viewerRightIdx >= 0 &&
                        state.viewerRightIdx < (int)state.images.size();
        bool hasBoth = hasLeft && hasRight;
        float fullW = ImGui::GetContentRegionAvail().x;

        // --- Markup section (renders its own header) ---
        ImageCanvas2D* lc = asCanvas2D(*state.viewerLeft);
        ImageCanvas2D* rc = asCanvas2D(*state.viewerRight);
        ImageCanvas2D* active =
            (state.activeComparisonViewer == 0)
                ? (hasLeft ? lc : (hasRight ? rc : nullptr))
                : (hasRight ? rc : (hasLeft ? lc : nullptr));
        ImageCanvas2D::renderMarkupControls(active);

        // --- Alignment section (only once both images load) ---
        if (!hasBoth) return;

        ImGui::SeparatorText("Alignment");

        if (ImGui::Button(state.viewerLocked ? "Unlock" : "Lock",
                          ImVec2(fullW, 0)))
            state.viewerLocked = !state.viewerLocked;

        if (ImGui::Button("Align", ImVec2(fullW, 0)))
        {
            ImageCanvas2D* lsel =
                asCanvas2D(*state.images[state.viewerLeftIdx]);
            ImageCanvas2D* rsel =
                asCanvas2D(*state.images[state.viewerRightIdx]);
            state.alignDialog.show = true;
            state.alignDialog.leftName =
                state.images[state.viewerLeftIdx]->name();
            state.alignDialog.rightName =
                state.images[state.viewerRightIdx]->name();
            state.alignDialog.leftImage = lsel ? lsel->image : nullptr;
            state.alignDialog.rightImage = rsel ? rsel->image : nullptr;
            state.viewerLocked = false;
            state.viewerLeft->resetView();
            state.viewerRight->resetView();
            state.viewerLocked = true;
        }

        ImGui::BeginDisabled(!state.viewerLocked);
        if (ImGui::Button("Save Alignment", ImVec2(fullW, 0)))
        {
            state.alignmentSaveBrowser.show = true;
            state.alignmentSaveBrowser.dirNeedsRefresh = true;
            state.alignmentSaveBrowser.fileName.clear();
        }
        ImGui::EndDisabled();

        if (!state.viewerLocked) return;

        ImGui::Spacing();
        auto& a = state.viewerAlignments[state.viewerAlignmentIdx];
        ImGui::TextWrapped(
            "[%d/%d] [%s]\nR:%.1f T:(%.0f,%.0f) S:%.2f",
            state.viewerAlignmentIdx + 1,
            (int)state.viewerAlignments.size(),
            a.mode == AlignMode::Manual ? "Manual" : "Auto",
            a.rotation / kDegToRad, a.dx, a.dy, a.scale);
        ImGui::Spacing();

        bool navDisabled = (int)state.viewerAlignments.size() <= 1;
        float halfW = (fullW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (navDisabled) ImGui::BeginDisabled();
        if (ImGui::Button("<", ImVec2(halfW, 0)))
        {
            if (state.viewerAlignmentIdx > 0)
            {
                state.viewerAlignmentIdx--;
                state.viewerLeft->applyAlignment(
                    *state.viewerRight,
                    state.viewerAlignments[state.viewerAlignmentIdx],
                    state.viewerLocked);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(">", ImVec2(halfW, 0)))
        {
            int maxIdx = (int)state.viewerAlignments.size() - 1;
            if (state.viewerAlignmentIdx < maxIdx)
            {
                state.viewerAlignmentIdx++;
                state.viewerLeft->applyAlignment(
                    *state.viewerRight,
                    state.viewerAlignments[state.viewerAlignmentIdx],
                    state.viewerLocked);
            }
        }
        if (navDisabled) ImGui::EndDisabled();

        if (navDisabled) ImGui::BeginDisabled();
        if (ImGui::Button("Delete", ImVec2(fullW, 0)))
        {
            int idx = state.viewerAlignmentIdx;
            state.viewerAlignments.erase(
                state.viewerAlignments.begin() + idx);
            if (idx >= (int)state.viewerAlignments.size())
                idx = (int)state.viewerAlignments.size() - 1;
            state.viewerAlignmentIdx = idx;
            state.viewerLeft->applyAlignment(
                *state.viewerRight, state.viewerAlignments[idx],
                state.viewerLocked);
        }
        if (navDisabled) ImGui::EndDisabled();

        if (ImGui::Button("Edit", ImVec2(fullW, 0)))
        {
            state.alignEditState =
                state.viewerAlignments[state.viewerAlignmentIdx];
            state.alignEditOriginal = state.alignEditState;
            state.alignEditOpen = true;
        }
    }

    void renderImageComparison(AppState& state)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float splitterW = 8.0f;
        float dockW = std::clamp(avail.x * state.dockRatio, 180.0f,
                                 avail.x * 0.4f);
        float contentW = avail.x - dockW - splitterW;

        renderComparisonContent(state, contentW, avail.y);

        // --- Right-side dock ---
        ImGui::SameLine();
        ImGui::Button("##DockSplitter", ImVec2(splitterW, avail.y));
        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            state.dockRatio -= delta / avail.x;
            state.dockRatio = std::clamp(state.dockRatio, 0.12f, 0.4f);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        ImGui::SameLine();
        ImGui::BeginChild("Dock", ImVec2(dockW, avail.y),
                          ImGuiChildFlags_Borders);
        // Roomier vertical rhythm so the stacked buttons don't look
        // cramped. Pushed here (not inside the dock fn) so the early
        // returns there can't unbalance the style stack.
        ImGui::PushStyleVar(
            ImGuiStyleVar_ItemSpacing,
            ImVec2(ImGui::GetStyle().ItemSpacing.x, 6.0f));
        renderComparisonDock(state);
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

}  // namespace shoecomp
