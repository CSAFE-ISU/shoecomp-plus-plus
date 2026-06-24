#ifndef SHOECOMP_IMAGE_LIST_DIALOG
#define SHOECOMP_IMAGE_LIST_DIALOG

#include <memory>
#include <vector>

namespace shoecomp
{
    struct ImageCanvas;

    struct ImageListDialog
    {
        bool show = false;
        void render(std::vector<std::unique_ptr<ImageCanvas>>& images,
                    int& viewerLeftIdx, int& viewerRightIdx,
                    int& activeGalleryImage);
    };
}  // namespace shoecomp

#endif
