#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/ResourceObject.h"

class SwapChain
{
public:
	virtual ~SwapChain() = default;

	virtual bool resize(uint32 width, uint32 height) = 0;
	virtual bool isRenderable() const = 0;
	virtual uint32 getCurrentImageIndex() const = 0;
	virtual ResourceObject* getCurrentBackBufferResource() = 0;
	virtual void present() = 0;
};
