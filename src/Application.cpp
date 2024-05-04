#include "Application.h"

void Application::run()
{
	init_window();
	init_vulkan();
	main_loop();
	cleanup();
}

void Application::init_window()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

void Application::init_vulkan()
{

}

void Application::main_loop()
{
    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();
    }
}

void Application::cleanup()
{
    glfwDestroyWindow(window);

    glfwTerminate();
}
