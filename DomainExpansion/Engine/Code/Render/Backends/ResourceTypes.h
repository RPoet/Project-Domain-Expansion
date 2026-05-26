#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/RenderTypes.h"

enum class ResourceObjectType : uint32
{
	unknown = 0,
	texture = 1,
	buffer = 2,
};

enum class BufferObjectMemoryType : uint32
{
	defaultHeap = 0,
	uploadHeap = 1,
	readbackHeap = 2,
};

enum class TextureDimension : uint32
{
	texture1D = 0,
	texture2D = 1,
	texture3D = 2,
};

enum class TextureLayout : uint32
{
	unknown = 0,
	rowMajor = 1,
	standardSwizzle64KB = 2,
	undefinedSwizzle64KB = 3,
};

enum class TextureObjectFlag : uint32
{
	none = 0,
	allowRenderTarget = 1,
	allowDepthStencil = 2,
	allowUnorderedAccess = 4,
	denyShaderResource = 8,
	allowCrossAdapter = 16,
	allowSimultaneousAccess = 32,
};

inline uint32 getTextureObjectFlag(const TextureObjectFlag flag)
{
	return static_cast<uint32>(flag);
}

