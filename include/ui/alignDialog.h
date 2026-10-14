#ifndef SHOECOMP_ALIGN_DIALOG
#define SHOECOMP_ALIGN_DIALOG

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "imgui.h"
#include "calc/align.h"
#include "core/alignment.h"
#include "core/params.h"
#include "jtjson/json.h"
#include "ui/imageCanvas2d.h"

namespace shoecomp
{
    // Map an image-coordinate point through |a| (right -> left) or
    // its inverse.
    ImVec2 transformRight2Left(const AlignState& a, const ImVec2& pt);
    ImVec2 transformLeft2Right(const AlignState& a, const ImVec2& pt);

    struct AlignDialog
    {
        bool show = false;
        bool open = false;
        std::string leftName;
        std::string rightName;
        std::shared_ptr<ImageCanvas2D::ImageData> leftImage;
        std::shared_ptr<ImageCanvas2D::ImageData> rightImage;
        std::vector<AlignState> workerResults;
        WorkerChannel channel;
        std::thread workerThread;
        bool workerFinished = false;
        AlignCalc::RTSParams rtsParams = Params().align2d;
        // Input type of the two images (EBTS adds the minutia rules).
        ImageType type = ImageType::Shoe;
        float minutiaAngleTolDeg = kDefaultMinutiaAngleTolDeg;
        char statusText[120]{};
        bool statusIsError = false;

        void render();
        void drainMessages();
        void startWorker();
        void cancelWorker();
        void cleanup();
    };
}  // namespace shoecomp

#endif
