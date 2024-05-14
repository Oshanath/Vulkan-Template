#include "TriangleRenderer.h"

int main() 
{
    std::vector<VkValidationFeatureEnableEXT> validation_features = { VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT };
    TriangleRenderer app("Vulkan Template", VK_API_VERSION_1_3, std::move(validation_features));

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;

}