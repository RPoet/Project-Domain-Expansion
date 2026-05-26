#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/RenderTypes.h"

class DepthStencilView
{
public:
	virtual ~DepthStencilView() = default;
	virtual TextureFormat getTextureFormat() const
	{
		return TextureFormat::unknown;
	}
};
