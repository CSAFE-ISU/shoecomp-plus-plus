#ifndef SHOECOMP_IMAGE_LIST_DIALOG
#define SHOECOMP_IMAGE_LIST_DIALOG

#include <vector>

namespace shoecomp
{
    struct LoadedImage;

    struct ImageListDialog
    {
        bool show = false;
        void render(std::vector<LoadedImage>& images,
                    int& viewerLeftIdx,
                    int& viewerRightIdx,
                    int& activeGalleryImage);
    };
}  // namespace shoecomp

#endif
