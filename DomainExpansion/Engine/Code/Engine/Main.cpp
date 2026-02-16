#include <iostream>

#include "Render/ResourceTypes.h"

int main()
{
    DomainExpansion::MeshAsset meshAsset;
    meshAsset.name = "DefaultMeshAsset";
    meshAsset.vertexCount = 3;
    meshAsset.indexCount = 3;

    DomainExpansion::MeshObject meshObject;
    meshObject.name = "DefaultMeshObject";
    meshObject.vertexBufferIdentifier = 1;
    meshObject.indexBufferIdentifier = 2;

    std::cout << "DomainExpansion Engine bootstrap started." << std::endl;
    std::cout << "CPU resource: " << meshAsset.name << ", vertices: " << meshAsset.vertexCount << std::endl;
    std::cout << "GPU resource: " << meshObject.name << ", vertex buffer id: " << meshObject.vertexBufferIdentifier << std::endl;

    return 0;
}
