#pragma once

#include <cstdint>
#include <string>

namespace DomainExpansion
{
    struct MeshAsset
    {
        std::string name;
        std::uint32_t vertexCount = 0;
        std::uint32_t indexCount = 0;
    };

    struct MeshObject
    {
        std::string name;
        std::uint32_t vertexBufferIdentifier = 0;
        std::uint32_t indexBufferIdentifier = 0;
    };
}
