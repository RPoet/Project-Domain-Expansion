#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/RenderTargetView.h"
#include "Render/ResourceObject.h"
#include "Render/ResourceState.h"

class CommandList
{
public:
	virtual ~CommandList() = default;

	virtual void beginRecord() = 0;
	virtual void resourceBarrier(
		ResourceObject* resourceObject,
		ResourceState beforeState,
		ResourceState afterState) = 0;
	virtual void setRenderTarget(RenderTargetView* renderTargetView) = 0;
	virtual void clearRenderTarget(
		RenderTargetView* renderTargetView,
		float red,
		float green,
		float blue,
		float alpha) = 0;
	virtual void flush() = 0;
};
