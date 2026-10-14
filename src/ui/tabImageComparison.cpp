#include "ui/mainWindow.h"
#include "ui/imageCanvas2d.h"
#include "ui/imageCanvas3d.h"
#include "ui/imageCanvasKinds.h"
#include "ui/uiHelpers.h"
#include "core/eulerAngles.h"
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
                    viewState.refitRequested = true;
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
    static std::shared_ptr<Surface3D> sourceSurface(AppState& state,
                                                    int idx);
    static X3PBulletLand* asBulletLand(ImageCanvas* c);
    static void setStriationLinks(AppState& state, X3PBulletLand& L,
                                  X3PBulletLand& R);

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

            bool is3D = state.viewerLeft && state.viewerLeft->is3D();
            const int prevLeftIdx = state.viewerLeftIdx;
            const int prevRightIdx = state.viewerRightIdx;
            if (is3D)
            {
                ImageCanvas3D* L = asCanvas3D(*state.viewerLeft);
                ImageCanvas3D* R = asCanvas3D(*state.viewerRight);
                if (L) L->compareXform = Eigen::Affine3f::Identity();
                if (R)
                {
                    R->compareXform = Eigen::Affine3f::Identity();
                    if (state.viewerLocked &&
                        !state.viewerAlignments3D.empty())
                    {
                        const Align3DState& a =
                            state.viewerAlignments3D
                                [state.viewerAlignmentIdx3D];
                        auto sl =
                            sourceSurface(state, state.viewerLeftIdx);
                        auto sr =
                            sourceSurface(state, state.viewerRightIdx);
                        if (!a.automatic)
                            R->compareXform = a.matrix();
                        else if (sl && sr)
                            // right model -> right physical -> left
                            // physical -> left model
                            R->compareXform =
                                (physicalToModel(*sl) *
                                 a.physical.inverse() *
                                 physicalToModel(*sr).inverse())
                                    .cast<float>();
                    }
                }
                X3PBulletLand* bl =
                    asBulletLand(state.viewerLeft.get());
                X3PBulletLand* br =
                    asBulletLand(state.viewerRight.get());
                if (bl && br) setStriationLinks(state, *bl, *br);
            }

            // Detect operates on the SOURCE gallery canvas (the 2D
            // comparison viewer is a plain Canvas2D with no spec; the
            // 3D viewer shares the source surface), so results land on
            // the shared data and show in both viewers.
            ImageCanvas* srcL =
                (state.viewerLeftIdx >= 0 &&
                 state.viewerLeftIdx < (int)state.images.size())
                    ? state.images[state.viewerLeftIdx].get()
                    : nullptr;
            state.viewerLeft->detectAvailable =
                detectionAvailable(srcL);
            ImGui::BeginChild("LeftViewer", ImVec2(leftW, 0),
                              ImGuiChildFlags_Borders);
            state.viewerLeft->renderViewerPanel(
                state.images, state.viewerLeftIdx, state.viewerRightIdx,
                state.viewerLocked, "##Left");
            ImGui::EndChild();
            if (state.viewerLeft->detectRequested)
            {
                state.viewerLeft->detectRequested = false;
                openDetect(state, srcL);
            }

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
            ImageCanvas* srcR =
                (state.viewerRightIdx >= 0 &&
                 state.viewerRightIdx < (int)state.images.size())
                    ? state.images[state.viewerRightIdx].get()
                    : nullptr;
            state.viewerRight->detectAvailable =
                detectionAvailable(srcR);
            ImGui::BeginChild("RightViewer", ImVec2(rightW, 0),
                              ImGuiChildFlags_Borders);
            state.viewerRight->renderViewerPanel(
                state.images, state.viewerRightIdx, state.viewerLeftIdx,
                state.viewerLocked, "##Right");
            ImGui::EndChild();
            if (state.viewerRight->detectRequested)
            {
                state.viewerRight->detectRequested = false;
                openDetect(state, srcR);
            }

            if (is3D && (state.viewerLeftIdx != prevLeftIdx ||
                         state.viewerRightIdx != prevRightIdx))
            {
                state.viewerAlignments3D = {Align3DState{}};
                state.viewerAlignmentIdx3D = 0;
            }

            if (is3D)
            {
                X3PBulletLand* bl =
                    asBulletLand(state.viewerLeft.get());
                X3PBulletLand* br =
                    asBulletLand(state.viewerRight.get());
                if (bl && br)
                {
                    auto partner = [](const X3PBulletLand& c)
                    {
                        const int h = c.hoveredStriation;
                        return (h >= 0 &&
                                h < (int)c.striationLinks.size())
                                   ? c.striationLinks[h]
                                   : -1;
                    };
                    bl->linkedStriation = partner(*br);
                    br->linkedStriation = partner(*bl);
                }
                if (state.viewerLocked)
                {
                    ImageCanvas3D* L = asCanvas3D(*state.viewerLeft);
                    ImageCanvas3D* R = asCanvas3D(*state.viewerRight);
                    if (L && R) L->sync3DCameras(*R, true);
                }
            }
            else if (state.viewerLocked && !state.alignEditPopupVisible)
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

    static X3PBulletLand* asBulletLand(ImageCanvas* c)
    {
        return (c && c->kind() == ImageCanvas::Kind::X3PBulletLand)
                   ? static_cast<X3PBulletLand*>(c)
                   : nullptr;
    }

    // Peaks[] partner maps for the striation hover link, from the
    // current automatic alignment (empty otherwise).
    static void setStriationLinks(AppState& state, X3PBulletLand& L,
                                  X3PBulletLand& R)
    {
        L.striationLinks.clear();
        R.striationLinks.clear();
        const ImVec4 c = state.settings.correspondingColor;
        L.linkColor = R.linkColor = ImVec4(c.x, c.y, c.z, 1.0f);
        if (!state.viewerLocked || state.viewerAlignments3D.empty())
            return;
        const Align3DState& a =
            state.viewerAlignments3D[state.viewerAlignmentIdx3D];
        if (!a.automatic || !a.info.isObject() ||
            !a.info.contains("matchedStriations") ||
            !a.info["matchedStriations"].isArray())
            return;
        auto setLink = [](std::vector<int>& links, int k, int partner)
        {
            if (k < 0) return;
            if ((int)links.size() <= k) links.resize(k + 1, -1);
            links[k] = partner;
        };
        for (const auto& pr : a.info["matchedStriations"].getArray())
        {
            if (!pr.isArray() || pr.getArray().size() != 2 ||
                !pr.getArray()[0].isNumber() ||
                !pr.getArray()[1].isNumber())
                continue;
            const int l = (int)pr.getArray()[0].getNumber();
            const int r = (int)pr.getArray()[1].getNumber();
            setLink(L.striationLinks, l, r);
            setLink(R.striationLinks, r, l);
        }
    }

    static std::shared_ptr<Surface3D> sourceSurface(AppState& state,
                                                    int idx)
    {
        if (idx < 0 || idx >= (int)state.images.size()) return nullptr;
        ImageCanvas3D* c = asCanvas3D(*state.images[idx]);
        return c ? c->surface : nullptr;
    }

    static void render3DAlignBox(AppState& state)
    {
        float fullW = ImGui::GetContentRegionAvail().x;
        float halfW = (fullW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        ImGui::SeparatorText("Alignment");

        bool hasLeft = state.viewerLeftIdx >= 0 &&
                       state.viewerLeftIdx < (int)state.images.size();
        bool hasRight = state.viewerRightIdx >= 0 &&
                        state.viewerRightIdx < (int)state.images.size();
        if (!(hasLeft && hasRight))
        {
            ImGui::TextWrapped("Select an image in each viewer.");
            return;
        }

        // Lock and Align share a row.
        if (dockButton(state.viewerLocked ? "Unlock" : "Lock",
                       ImVec2(halfW, 0)))
            state.viewerLocked = !state.viewerLocked;
        ImGui::SameLine();
        const bool canAlign =
            state.images[state.viewerLeftIdx]->kind() ==
                ImageCanvas::Kind::X3PBulletLand &&
            state.images[state.viewerRightIdx]->kind() ==
                ImageCanvas::Kind::X3PBulletLand;
        ImGui::BeginDisabled(!canAlign);
        if (dockButton("Align", ImVec2(halfW, 0)))
        {
            ImageCanvas3D* l =
                asCanvas3D(*state.images[state.viewerLeftIdx]);
            ImageCanvas3D* r =
                asCanvas3D(*state.images[state.viewerRightIdx]);
            if (l && r && !state.align3DDialog.openFor(*l, *r))
            {
                state.align3DError.show = true;
                state.align3DError.message =
                    std::string(state.align3DDialog.statusText);
            }
        }
        ImGui::EndDisabled();
        if (!canAlign &&
            ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(
                "Automatic alignment matches striations "
                "on bullet-land surfaces.");

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
        Align3DState& a =
            state.viewerAlignments3D[state.viewerAlignmentIdx3D];
        ImGui::Text(u8"Alignment %d/%d \u00B7 %s",
                    state.viewerAlignmentIdx3D + 1,
                    (int)state.viewerAlignments3D.size(),
                    a.automatic ? "Auto" : "Manual");

        if (a.automatic)
        {
            const Eigen::Vector3f e =
                matrixToEuler(a.physical.linear().cast<float>());
            const Eigen::Vector3d t = a.physical.translation() * 1e6;
            const jt::Json& info = a.info;
            int matched = 0;
            double rms = 0.0;
            bool axialWeak = false;
            if (info.isObject())
            {
                if (info.contains("matchedStriations") &&
                    info["matchedStriations"].isArray())
                    matched = (int)info["matchedStriations"]
                                  .getArray()
                                  .size();
                if (info.contains("rmsPerpUm") &&
                    info["rmsPerpUm"].isNumber())
                    rms = info["rmsPerpUm"].getNumber();
                if (info.contains("axialWeak") &&
                    info["axialWeak"].isBool())
                    axialWeak = info["axialWeak"].getBool();
            }
            if (ImGui::BeginTable("##align3dInfo", 2,
                                  ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(u8"\u03B8");  // theta: rotation
                ImGui::TableNextColumn();
                ImGui::Text(u8"%.2f, %.2f, %.2f\u00B0", e.x(), e.y(),
                            e.z());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("pitch, yaw, roll (degrees)");

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(
                    u8"\u0394");  // delta: translation
                ImGui::TableNextColumn();
                ImGui::Text(u8"%.1f, %.1f, %.1f \u00B5m", t.x(), t.y(),
                            t.z());

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Lines");
                ImGui::TableNextColumn();
                ImGui::Text(u8"%d \u00B7 rms %.2f \u00B5m", matched,
                            rms);
                ImGui::EndTable();
            }
            if (axialWeak)
            {
                ImGui::TextWrapped("Axial offset from midpoints only");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "The matched striations are nearly parallel, "
                        "so the shift along them\ncomes from their "
                        "detected extents and is less certain.");
            }
        }
        else
        {
            ImGui::SetNextItemWidth(fullW);
            ImGui::DragFloat3("##t3d", a.translation.data(), 0.005f,
                              -5.0f, 5.0f, "%.3f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Translate (model units)");
            ImGui::SetNextItemWidth(fullW);
            ImGui::DragFloat3("##r3d", a.rotationDeg.data(), 0.5f,
                              -180.0f, 180.0f, u8"%.1f°");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Rotate: pitch, yaw, roll (degrees)");
            ImGui::SetNextItemWidth(fullW);
            ImGui::DragFloat("##s3d", &a.scale, 0.01f, 0.1f, 5.0f,
                             "%.2f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale");
        }

        ImGui::Spacing();
        bool navDisabled = (int)state.viewerAlignments3D.size() <= 1;
        if (navDisabled) ImGui::BeginDisabled();
        if (dockButton("<", ImVec2(halfW, 0)) &&
            state.viewerAlignmentIdx3D > 0)
            state.viewerAlignmentIdx3D--;
        ImGui::SameLine();
        if (dockButton(">", ImVec2(halfW, 0)) &&
            state.viewerAlignmentIdx3D <
                (int)state.viewerAlignments3D.size() - 1)
            state.viewerAlignmentIdx3D++;
        if (navDisabled) ImGui::EndDisabled();

        if (dockButton("Add", ImVec2(halfW, 0)))
        {
            state.viewerAlignments3D.push_back(Align3DState{});
            state.viewerAlignmentIdx3D =
                (int)state.viewerAlignments3D.size() - 1;
        }
        ImGui::SameLine();
        if (navDisabled) ImGui::BeginDisabled();
        if (dockButton("Delete", ImVec2(halfW, 0)))
        {
            int idx = state.viewerAlignmentIdx3D;
            state.viewerAlignments3D.erase(
                state.viewerAlignments3D.begin() + idx);
            if (idx >= (int)state.viewerAlignments3D.size())
                idx = (int)state.viewerAlignments3D.size() - 1;
            state.viewerAlignmentIdx3D = idx;
        }
        if (navDisabled) ImGui::EndDisabled();
    }

    static void renderAlignmentBox(AppState& state)
    {
        if (state.viewerLeft && state.viewerLeft->is3D())
        {
            render3DAlignBox(state);
            return;
        }

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
            ImageType type = ImageType::Shoe;
            imageTypeForKind(state.images[state.viewerLeftIdx]->kind(),
                             type);
            state.alignDialog.type = type;
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
