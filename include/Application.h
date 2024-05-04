#ifndef APPLICATION_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

class Application 
{
public:
    void run();

private:
    GLFWwindow* window;
    uint32_t WIDTH = 1920;
    uint32_t HEIGHT = 1080;

    void init_vulkan();

    void main_loop();

    void cleanup();

    void init_window();
};

#endif // !APPLICATION_H
