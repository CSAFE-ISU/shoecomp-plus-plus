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
        float toolbarH = ImGui::GetFrameHeightWithSpacing() * 2.0f;
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

    static void renderAlignmentBox(AppState& state);

    static void renderComparisonContent(AppState& state, float contentW,
                                        float contentH)
    {
        ImGui::BeginChild("ComparisonContent",
                          ImVec2(contentW, contentH),
                          ImGuiChildFlags_None);
        {
            float totalW = ImGui::GetContentRegionAvail().x;
            float fullH = ImGui::GetContentRegionAvail().y;
            float splitterW = 8.0f;
            float boxW = std::clamp(totalW * state.alignBoxRatio,
                                    200.0f, totalW * 0.5f);
            float sideTotal =
                std::max(0.0f, totalW - boxW - 2.0f * splitterW);
            float leftW = sideTotal * state.viewerSplitRatio;
            float rightW = sideTotal - leftW;

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

            // Splitter between the left viewer and the box: resizes
            // the two viewers against each other.
            ImGui::SameLine();
            ImGui::Button("##splitL", ImVec2(splitterW, fullH));
            if (ImGui::IsItemActive() && sideTotal > 1.0f)
            {
                state.viewerSplitRatio +=
                    ImGui::GetIO().MouseDelta.x / sideTotal;
                state.viewerSplitRatio =
                    std::clamp(state.viewerSplitRatio, 0.1f, 0.9f);
            }
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

            // Alignment box: fixed height, centered vertically in its
            // column, with draggable width (the splitters flanking it).
            ImGui::SameLine();
            ImGui::BeginChild("AlignColumn", ImVec2(boxW, 0),
                              ImGuiChildFlags_None);
            {
                float availH = ImGui::GetContentRegionAvail().y;
                float boxH = std::min(
                    availH, ImGui::GetFrameHeightWithSpacing() * 11.0f);
                float offY = (availH - boxH) * 0.5f;
                if (offY > 0.0f) ImGui::Dummy(ImVec2(0.0f, offY));
                ImGui::BeginChild("AlignBox", ImVec2(0, boxH),
                                  ImGuiChildFlags_Borders);
                ImGui::PushStyleVar(
                    ImGuiStyleVar_ItemSpacing,
                    ImVec2(ImGui::GetStyle().ItemSpacing.x, 6.0f));
                renderAlignmentBox(state);
                ImGui::PopStyleVar();
                ImGui::EndChild();
            }
            ImGui::EndChild();

            // Splitter between the box and the right viewer: resizes
            // the alignment box width.
            ImGui::SameLine();
            ImGui::Button("##splitR", ImVec2(splitterW, fullH));
            if (ImGui::IsItemActive() && totalW > 1.0f)
            {
                state.alignBoxRatio +=
                    ImGui::GetIO().MouseDelta.x / totalW;
                state.alignBoxRatio =
                    std::clamp(state.alignBoxRatio, 0.1f, 0.4f);
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

    static void renderAlignmentBox(AppState& state)
    {
        bool hasLeft = state.viewerLeftIdx >= 0 &&
                       state.viewerLeftIdx < (int)state.images.size();
        bool hasRight = state.viewerRightIdx >= 0 &&
                        state.viewerRightIdx < (int)state.images.size();
        bool hasBoth = hasLeft && hasRight;
        float fullW = ImGui::GetContentRegionAvail().x;

        ImGui::SeparatorText("Alignment");

        // Alignment needs both viewers populated.
        if (!hasBoth)
        {
            ImGui::TextWrapped("Select an image in each viewer.");
            return;
        }

        float halfW = (fullW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        // Lock and Align share a row.
        if (dockButton(state.viewerLocked ? "Unlock" : "Lock",
                       ImVec2(halfW, 0)))
            state.viewerLocked = !state.viewerLocked;
        ImGui::SameLine();
        if (dockButton("Align", ImVec2(halfW, 0)))
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
        if (dockButton("Save Alignment", ImVec2(fullW, 0)))
        {
            state.alignmentSaveBrowser.show = true;
            state.alignmentSaveBrowser.dirNeedsRefresh = true;
            state.alignmentSaveBrowser.fileName.clear();
        }
        ImGui::EndDisabled();

        if (!state.viewerLocked) return;

        ImGui::Spacing();
        auto& a = state.viewerAlignments[state.viewerAlignmentIdx];
        ImGui::Text(u8"Alignment %d/%d \u00B7 %s",
                    state.viewerAlignmentIdx + 1,
                    (int)state.viewerAlignments.size(),
                    a.mode == AlignMode::Manual ? "Manual" : "Auto");
        if (ImGui::BeginTable("##alignInfo", 2,
                              ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(u8"\u03B8");  // theta: rotation
            ImGui::TableNextColumn();
            ImGui::Text(u8"%.1f\u00B0", a.rotation / kDegToRad);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(u8"\u0394");  // delta: translation
            ImGui::TableNextColumn();
            ImGui::Text("%.0f, %.0f", a.dx, a.dy);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Scale");
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", a.scale);
            ImGui::EndTable();
        }
        ImGui::Spacing();

        bool navDisabled = (int)state.viewerAlignments.size() <= 1;

        // Previous / next alignment.
        if (navDisabled) ImGui::BeginDisabled();
        if (dockButton("<", ImVec2(halfW, 0)))
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
        if (dockButton(">", ImVec2(halfW, 0)))
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

        // Edit and Delete share a row (Delete needs >1 alignment).
        if (dockButton("Edit", ImVec2(halfW, 0)))
        {
            state.alignEditState =
                state.viewerAlignments[state.viewerAlignmentIdx];
            state.alignEditOriginal = state.alignEditState;
            state.alignEditOpen = true;
        }
        ImGui::SameLine();
        if (navDisabled) ImGui::BeginDisabled();
        if (dockButton("Delete", ImVec2(halfW, 0)))
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
    }

    void renderImageComparison(AppState& state)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        renderComparisonContent(state, avail.x, avail.y);
    }

}  // namespace shoecomp
