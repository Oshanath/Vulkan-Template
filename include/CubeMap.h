#ifndef CUBE_MAP_H
#define CUBE_MAP_H

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include <memory>
#include "Model.h"

class CubeMap
{
public:
	CubeMap(std::shared_ptr<vpp::Backend> backend);
};

#endif // !CUBE_MAP_H
