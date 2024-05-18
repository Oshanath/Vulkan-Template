#include "CubeMap.h"
#include <iostream>

CubeMap::CubeMap(std::shared_ptr<vpp::Backend> backend)
{
    std::cout << "CubeMap constructor" << std::endl;

    //vpp::Model model("models/skyBox/sky.glb", backend, vpp::TextureType::EMBEDDED);
}