#include "TriangleRenderer.h"

int main() 
{
    TriangleRenderer app("Vulkan Template");

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;

}