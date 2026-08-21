#include "ui/mainWindow.h"
#include "ui/imageCanvas2d.h"
#include "ui/imageCanvasKinds.h"
#include "ui/alignDialog.h"
#include "calc/onnxRuntime.h"
#include "ui/uiHelpers.h"
#include "ui/splash.h"
#include "ui/licenseData.h"
#include "ui/embeddedAssets.h"
#include "ui/icons.h"
#include "formats/png.h"
#include "jtjson/json.h"
#include "hello_imgui/imgui_theme.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace shoecomp
{
    namespace fs = std::filesystem;

    static void renderImageSaveProgressPopup(AppState& state)
    {
        if (state.imageSaveInProgress.load())
        {
            ImGui::OpenPopup("Saving Image...");
        }
        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(ds.x * 0.4f, ds.y * 0.2f),
                                 ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.3f, ds.y * 0.4f),
                                ImGuiCond_Always);
        if (ImGui::BeginPopupModal(
                "Saving Image...", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("Saving to: %s",
                        state.imageSaveProgressPath.c_str());
            ImGui::Spacing();

            // Animated spinner dots
            int dots = (int)(ImGui::GetTime() / 0.4) % 3 + 1;
            ImGui::Text("Please wait%.*s", dots, "...");

            if (state.imageSaveDone.load())
            {
                if (state.imageSaveThread.joinable())
                    state.imageSaveThread.join();
                if (state.imageSaveResult.load() != 0)
                {
                    state.imageSaveError.show = true;
                    state.imageSaveError.message =
                        "Failed to save image "
                        "to:\n" +
                        state.imageSaveProgressPath;
                }
                state.imageSaveInProgress = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    static void consumeAlignResults(AppState& state)
    {
        if (!state.alignDialog.open &&
            state.alignDialog.workerFinished &&
            !state.alignDialog.workerResults.empty())
        {
            size_t count = state.alignDialog.workerResults.size();
            for (auto& a : state.alignDialog.workerResults)
                state.viewerAlignments.push_back(std::move(a));
            state.viewerAlignmentIdx =
                (int)state.viewerAlignments.size() - 1;
            state.alignDialog.workerResults.clear();
            state.alignDialog.workerFinished = false;
            state.viewerLeft->applyAlignment(
                *state.viewerRight,
                state.viewerAlignments[state.viewerAlignmentIdx],
                state.viewerLocked);
            printf(
                "mainWindow: %zu alignment(s) "
                "added\n",
                count);
        }
    }

    static void renderAlignEditPopup(AppState& state)
    {
        if (state.alignEditOpen)
        {
            ImGui::OpenPopup("Edit Alignment");
            state.alignEditOpen = false;
        }
        ImVec2 eds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(ImVec2(eds.x * 0.35f, eds.y * 0.35f),
                                 ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(eds.x * 0.325f, eds.y * 0.325f),
                                ImGuiCond_Appearing);
        state.alignEditPopupVisible = false;
        if (ImGui::BeginPopupModal(
                "Edit Alignment", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
        {
            state.alignEditPopupVisible = true;
            bool changed = false;
            float rotationDeg =
                state.alignEditState.rotation / kDegToRad;
            changed |=
                ImGui::SliderFloat("Rotation (deg)", &rotationDeg,
                                   -180.0f, 180.0f, "%.1f");
            changed |= ImGui::SliderFloat("Translation X",
                                          &state.alignEditState.dx,
                                          -2000.0f, 2000.0f, "%.1f");
            changed |= ImGui::SliderFloat("Translation Y",
                                          &state.alignEditState.dy,
                                          -2000.0f, 2000.0f, "%.1f");
            changed |=
                ImGui::SliderFloat("Scale", &state.alignEditState.scale,
                                   0.1f, 10.0f, "%.2f");

            if (changed)
            {
                state.alignEditState.rotation = rotationDeg * kDegToRad;
                state.viewerLeft->applyAlignment(*state.viewerRight,
                                                 state.alignEditState,
                                                 state.viewerLocked);
            }

            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Add"))
            {
                state.viewerAlignments.push_back(state.alignEditState);
                state.viewerAlignmentIdx =
                    state.viewerAlignments.size() - 1;
                state.viewerLeft->applyAlignment(
                    *state.viewerRight,
                    state.viewerAlignments[state.viewerAlignmentIdx],
                    state.viewerLocked);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Replace"))
            {
                state.viewerAlignments[state.viewerAlignmentIdx] =
                    state.alignEditState;
                state.viewerLeft->applyAlignment(*state.viewerRight,
                                                 state.alignEditState,
                                                 state.viewerLocked);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                state.viewerAlignments[state.viewerAlignmentIdx] =
                    state.alignEditOriginal;
                state.viewerLeft->applyAlignment(
                    *state.viewerRight, state.alignEditOriginal,
                    state.viewerLocked);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // Soft reset when the active canvas kind changes: close every
    // image not of the new kind, reset comparison/selection state,
    // and rebuild the active prototype + picker filter.
    static void applyActiveKindChange(AppState& state)
    {
        if (state.settings.activeKind == state.lastActiveKind) return;
        state.lastActiveKind = state.settings.activeKind;

        state.images.erase(
            std::remove_if(
                state.images.begin(), state.images.end(),
                [&](const std::unique_ptr<ImageCanvas>& c)
                { return c->kind() != state.settings.activeKind; }),
            state.images.end());

        state.viewerLeftIdx = -1;
        state.viewerRightIdx = -1;
        state.activeGalleryImage = -1;
        state.activeComparisonViewer = 0;
        ImageCanvas2D::annotationMode =
            ImageCanvas2D::AnnotationMode::None;
        state.viewerLocked = false;
        state.viewerAlignments = {AlignState{}};
        state.viewerAlignmentIdx = 0;
        state.alignEditOpen = false;
        state.alignEditPopupVisible = false;
        state.viewerLeft = std::make_unique<ImageCanvas2D>();
        state.viewerRight = std::make_unique<ImageCanvas2D>();

        state.activeProto = makeCanvas(state.settings.activeKind);
        state.imageLoadBrowser.extensionChoices =
            state.activeProto->imageExtensions();

        // Opportunistically (re)check for the optional onnxruntime
        // library whenever the canvas kind changes. Idempotent and
        // never throws; failure just leaves detection disabled.
        OnnxRuntime::instance().ensureLoaded();
    }

    static void renderFileMenu(AppState& state)
    {
        bool hasActive =
            state.activeGalleryImage >= 0 &&
            state.activeGalleryImage < (int)state.images.size();

        if (ImGui::MenuItem("Load Image..."))
            openImagePicker(state.imageLoadBrowser);
#ifdef __EMSCRIPTEN__
        if (ImGui::IsItemHovered())
            setLoadBrowserHovered(&state.imageLoadBrowser);
#endif

        if (ImGui::MenuItem("Save PNG...", nullptr, false, hasActive))
        {
            state.imageSaveBrowser.show = true;
            state.imageSaveTarget = state.activeGalleryImage;
            state.imageSaveBrowser.dirNeedsRefresh = true;
            state.imageSaveBrowser.fileName.clear();
        }

        if (ImGui::MenuItem("Load Annotations...", nullptr, false,
                            hasActive))
        {
            state.annotationFileSave = false;
            state.annotationFileTarget = state.activeGalleryImage;
#ifdef __EMSCRIPTEN__
            openJsonPicker(state.annotationFileBrowser);
#else
            state.annotationFileBrowser.dirNeedsRefresh = true;
            state.annotationFileBrowser.fileName.clear();
            state.annotationFileBrowser.contextLabel.clear();
            state.annotationFileBrowser.extensionChoices = {".json",
                                                            ""};
            state.annotationFileBrowser.title = "Load Annotations";
            state.annotationFileBrowser.show = true;
#endif
        }

        if (ImGui::MenuItem("Save Annotations...", nullptr, false,
                            hasActive))
        {
#ifdef __EMSCRIPTEN__
            auto& img =
                asCanvas2D(*state.images[state.activeGalleryImage])
                    ->image;
            jt::Json copy = img->annotations;
            std::string data = copy.toStringPretty();
            if (data.empty())
            {
                state.annotationError.show = true;
                state.annotationError.message =
                    "Failed to serialize annotations.";
            }
            else
            {
                std::string fname =
                    img->name.empty() ? std::string("annotations.json")
                                      : img->name + ".json";
                downloadJsonString(data.c_str(), (int)data.size(),
                                   fname.c_str(), (int)fname.size());
            }
#else
            state.annotationFileBrowser.show = true;
            state.annotationFileSave = true;
            state.annotationFileTarget = state.activeGalleryImage;
            state.annotationFileBrowser.dirNeedsRefresh = true;
            state.annotationFileBrowser.fileName.clear();
            {
                ImageCanvas2D* c2d =
                    asCanvas2D(*state.images[state.activeGalleryImage]);
                state.annotationFileBrowser.contextLabel =
                    (c2d && c2d->image) ? c2d->image->name
                                        : std::string();
            }
            state.annotationFileBrowser.extensionChoices.clear();
            state.annotationFileBrowser.title = "Save Annotations";
#endif
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Images...", nullptr, false,
                            !state.images.empty()))
            state.imageListDialog.show = true;
    }

    static void renderMenus(AppState& state)
    {
        if (ImGui::BeginMenu("shoecomp"))
        {
            ImGui::SeparatorText("Workspaces");
            if (ImGui::MenuItem("Image Viewer", nullptr,
                                state.workspace == Workspace::Viewer))
                state.workspace = Workspace::Viewer;
            if (ImGui::MenuItem(
                    "Image Comparison", nullptr,
                    state.workspace == Workspace::Comparison))
                state.workspace = Workspace::Comparison;

            ImGui::SeparatorText("File");
            renderFileMenu(state);
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Settings", nullptr,
                            state.workspace == Workspace::Settings))
            state.workspace = Workspace::Settings;
        if (ImGui::MenuItem("About", nullptr,
                            state.workspace == Workspace::About))
            state.workspace = Workspace::About;
    }

    static void renderGui(AppState& state)
    {
        applyActiveKindChange(state);
        state.imageLoadError.render();
        state.annotationError.render();
        state.imageSaveError.render();
        state.alignmentSaveError.render();
        state.alignmentSaveBrowser.render();
        state.imageLoadBrowser.render();
        state.imageSaveBrowser.render();
        state.annotationFileBrowser.render();
        renderImageSaveProgressPopup(state);
        {
            int prevLeft = state.viewerLeftIdx;
            int prevRight = state.viewerRightIdx;
            state.imageListDialog.render(
                state.images, state.viewerLeftIdx, state.viewerRightIdx,
                state.activeGalleryImage);
            if (prevLeft != state.viewerLeftIdx ||
                prevRight != state.viewerRightIdx)
            {
                state.viewerLocked = false;
                state.viewerAlignments = {AlignState{}};
                state.viewerAlignmentIdx = 0;
                state.alignEditOpen = false;
                state.alignEditPopupVisible = false;
                state.viewerLeftIdx = -1;
                state.viewerRightIdx = -1;
                state.viewerLeft = std::make_unique<ImageCanvas2D>();
                state.viewerRight = std::make_unique<ImageCanvas2D>();
            }
        }

        state.alignDialog.render();
        consumeAlignResults(state);
        renderAlignEditPopup(state);

        state.detectDialog.render();
        state.detectError.render();

        switch (state.workspace)
        {
            case Workspace::Viewer:
                renderImageGallery(state);
                break;
            case Workspace::Comparison:
                renderImageComparison(state);
                break;
            case Workspace::Settings:
                renderFilesAndSettings(state);
                break;
            case Workspace::About:
                renderAbout(state);
                break;
        }
    }

    static void onImageSaveOk(AppState& state,
                              const std::string& fullPath)
    {
        if (state.imageSaveTarget < 0 ||
            state.imageSaveTarget >= (int)state.images.size() ||
            state.imageSaveInProgress.load())
            return;
        ImageCanvas2D* c2d =
            asCanvas2D(*state.images[state.imageSaveTarget]);
        if (!c2d) return;
        auto& img = c2d->image;

        int w = img->width;
        int h = img->height;
        auto buffer =
            std::make_shared<std::vector<unsigned char>>(w * h * 4);
        if (!saveTextureRGBA(img->textureId, w, h, buffer->data()))
            return;

        state.imageSaveInProgress = true;
        state.imageSaveDone = false;
        state.imageSaveProgressPath = fullPath;
        if (state.imageSaveThread.joinable())
            state.imageSaveThread.join();
        state.imageSaveThread = std::thread(
            [&state, fullPath, buffer, w, h]()
            {
                int res = savePngToDisk(fullPath, buffer->data(), w, h);
                state.imageSaveResult = res;
                state.imageSaveDone = true;
            });
    }

    static void onAnnotationFileOk(AppState& state,
                                   const std::string& fullPath)
    {
        if (state.annotationFileTarget < 0 ||
            state.annotationFileTarget >= (int)state.images.size())
            return;
        ImageCanvas2D* c2d =
            asCanvas2D(*state.images[state.annotationFileTarget]);
        if (!c2d) return;
        if (state.annotationFileSave)
        {
            if (c2d->saveAnnotations(fullPath) != 0)
            {
                state.annotationError.show = true;
                state.annotationError.message =
                    "Failed to save "
                    "annotations to:\n" +
                    fullPath;
            }
        }
        else
        {
            std::string annErr;
            if (c2d->loadAnnotations(fullPath, annErr) != 0)
            {
                state.annotationError.show = true;
                state.annotationError.message = annErr;
            }
        }
    }

    static void onAlignmentSaveOk(AppState& state,
                                  const std::string& fullPath)
    {
        auto& a = state.viewerAlignments[state.viewerAlignmentIdx];
        jt::Json obj;
        obj.setObject();
        obj["mode"] = jt::Json(
            a.mode == AlignMode::Manual ? "Manual" : "Automatic");
        obj["rotation"] = jt::Json((double)a.rotation);
        obj["dx"] = jt::Json((double)a.dx);
        obj["dy"] = jt::Json((double)a.dy);
        obj["scale"] = jt::Json((double)a.scale);
        obj["info"] = a.info;

        std::ofstream ofs(fullPath);
        if (!ofs)
        {
            state.alignmentSaveError.show = true;
            state.alignmentSaveError.message =
                "Failed to save alignment "
                "to:\n" +
                fullPath;
            return;
        }
        ofs << obj.toStringPretty();
        if (!ofs)
        {
            state.alignmentSaveError.show = true;
            state.alignmentSaveError.message =
                "Failed to write alignment "
                "to:\n" +
                fullPath;
        }
    }

    static void onImageLoadSelect(AppState& state,
                                  const std::string& fullPath,
                                  const std::string& /*name*/)
    {
        for (auto& c : state.images)
        {
            if (c->path() == fullPath) return;
        }

        // The active-kind prototype decides which formats are
        // accepted and constructs the right canvas subclass(es).
        std::vector<std::unique_ptr<ImageCanvas>> loaded;
        std::string err;
        int n = state.activeProto->loadImages(fullPath, loaded, err);
        if (n <= 0)
        {
            state.imageLoadError.show = true;
            state.imageLoadError.message =
                err.empty() ? "Failed to load image from:\n" + fullPath
                            : err;
            return;
        }

        // Auto-load a sibling .json for a single raster image.
        if (n == 1 && state.imageLoadBrowser.loadCorrespondingJson)
        {
            ImageCanvas2D* c2d = asCanvas2D(*loaded[0]);
            fs::path jsonPath =
                fs::path(fullPath).replace_extension(".json");
            if (c2d && fs::exists(jsonPath))
            {
                std::string annErr;
                if (c2d->loadAnnotations(jsonPath.string(), annErr) !=
                    0)
                {
                    c2d->image->resetAnnotations();
                    state.annotationError.show = true;
                    state.annotationError.message = annErr;
                }
            }
        }

        for (auto& c : loaded) state.images.push_back(std::move(c));
    }

    static void onBeforeExit(AppState& state)
    {
        if (const char* ini = ImGui::GetIO().IniFilename)
            ImGui::SaveIniSettingsToDisk(ini);

        if (state.imageSaveThread.joinable())
            state.imageSaveThread.join();
        state.alignDialog.cancelWorker();
        state.alignDialog.cleanup();
        state.alignDialog.leftImage.reset();
        state.alignDialog.rightImage.reset();
        state.viewerLeft.reset();
        state.viewerRight.reset();
        state.images.clear();
    }

    static void loadStaticAssets(AppState& state)
    {
        ImGuiIO& io = ImGui::GetIO();
        float dpi = HelloImGui::DpiFontLoadingFactor();
        state.defaultFont = io.Fonts->AddFontFromMemoryCompressedTTF(
            MontserratRegular_compressed_data,
            (int)MontserratRegular_compressed_size, 16.0f * dpi);
        state.boldFont = io.Fonts->AddFontFromMemoryCompressedTTF(
            MontserratSemiBold_compressed_data,
            (int)MontserratSemiBold_compressed_size, 16.0f * dpi);
        state.monoFont = io.Fonts->AddFontFromMemoryCompressedTTF(
            InconsolataRegular_compressed_data,
            (int)InconsolataRegular_compressed_size, 16.0f * dpi);

        static const ImWchar iconRanges[] = {kIconRangeMin,
                                             kIconRangeMax, 0};
        ImFontConfig iconCfg;
        iconCfg.GlyphMinAdvanceX = 18.0f * dpi;
        state.iconFont = io.Fonts->AddFontFromMemoryCompressedTTF(
            MaterialIcons_compressed_data,
            (int)MaterialIcons_compressed_size, 18.0f * dpi, &iconCfg,
            iconRanges);
    }

    void submain(void)
    {
#ifndef __EMSCRIPTEN__
        SplashResult splashResult = runSplash(1.0);
        if (splashResult.cancelled) return;
#endif

        AppState state;
        state.viewerLeft = std::make_unique<ImageCanvas2D>();
        state.viewerRight = std::make_unique<ImageCanvas2D>();
        state.activeProto = makeCanvas(state.settings.activeKind);
        state.lastActiveKind = state.settings.activeKind;
#ifndef __EMSCRIPTEN__
        state.licenseTexts = std::move(splashResult.licenseTexts);
#else
        state.licenseTexts = preloadLicenseTexts();
#endif

        state.imageLoadError.title = "Image Load Error";
        state.imageSaveError.title = "Image Save Error";
        state.annotationError.title = "Annotation Error";
        state.detectError.title = "Detection Error";

        // First search for the optional onnxruntime library so the
        // Detect Points button reflects availability from the start.
        OnnxRuntime::instance().ensureLoaded();

        state.imageSaveBrowser.extension = ".png";
        state.imageSaveBrowser.title = "Save Image";
        state.imageSaveBrowser.onOk = [&state](const std::string& p)
        { onImageSaveOk(state, p); };

        state.annotationFileBrowser.extension = ".json";
        state.annotationFileBrowser.title = "Annotation File";
        state.annotationFileBrowser.onOk =
            [&state](const std::string& p)
        { onAnnotationFileOk(state, p); };

        state.alignmentSaveBrowser.extension = ".json";
        state.alignmentSaveBrowser.title = "Save Alignment";
        state.alignmentSaveError.title = "Alignment Save Error";
        state.alignmentSaveBrowser.onOk = [&state](const std::string& p)
        { onAlignmentSaveOk(state, p); };

        state.imageLoadBrowser.extension = ".png";
        state.imageLoadBrowser.extensionChoices =
            state.activeProto->imageExtensions();
        state.imageLoadBrowser.title = "Load Image";
        state.imageLoadBrowser.onSelect =
            [&state](const std::string& p, const std::string& n)
        { onImageLoadSelect(state, p, n); };

        HelloImGui::RunnerParams params;
        params.appWindowParams.windowTitle = "ShoeComp";
        params.appWindowParams.windowGeometry.fullScreenMode =
            HelloImGui::FullScreenMode::NoFullScreen;
        params.appWindowParams.windowGeometry.windowSizeState =
            HelloImGui::WindowSizeState::Maximized;
        params.imGuiWindowParams.defaultImGuiWindowType =
            HelloImGui::DefaultImGuiWindowType::ProvideFullScreenWindow;
        params.imGuiWindowParams.showMenuBar = true;
        params.imGuiWindowParams.showMenu_App = false;
        params.imGuiWindowParams.showMenu_View = false;
        params.callbacks.ShowMenus = [&state]() { renderMenus(state); };
        params.callbacks.LoadAdditionalFonts = [&state]()
        { loadStaticAssets(state); };
        params.callbacks.PostInit = [&state]()
        {
            registerSettingsHandler(state.settings);
            if (const char* ini = ImGui::GetIO().IniFilename)
                ImGui::LoadIniSettingsFromDisk(ini);
            ImGui::GetIO().FontGlobalScale = state.settings.fontScale;
            applyTheme(state.settings.themeIdx);
        };
        params.callbacks.ShowGui = [&state]()
        {
            applyTheme(state.settings.themeIdx);
            renderGui(state);
        };
        params.callbacks.BeforeExit = [&state]()
        { onBeforeExit(state); };

        HelloImGui::Run(params);
    }

}  // namespace shoecomp
