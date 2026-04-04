#ifndef SHOECOMP_IMAGE_LIST_DIALOG
#define SHOECOMP_IMAGE_LIST_DIALOG

#include <vector>

namespace shoecomp
{
    struct ImageCanvas;

    struct ImageListDialog
    {
        bool show = false;
        void render(std::vector<ImageCanvas>& images,
                    int& viewerLeftIdx, int& viewerRightIdx,
                    int& activeGalleryImage);
    };
}  // namespace shoecomp

#endif
