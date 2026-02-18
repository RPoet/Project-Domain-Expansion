#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/RenderTargetView.h"
#include "Render/ResourceObject.h"
#include "Render/ResourceState.h"

enum class CommandListType : uint32
{
	graphics = 0,
	compute = 1,
	copy = 2,
};

struct CommandListInitializeOptions
{
	void* nativeGraphicsDevice = nullptr;
	CommandListType commandListType = CommandListType::graphics;
};

class CommandList
{
public:
	virtual ~CommandList() = default;

	virtual bool initialize(const CommandListInitializeOptions& initializeOptions) = 0;
	virtual void shutdown() = 0;
	virtual void reset() = 0;
	virtual void resourceBarrier(
		ResourceObject* resourceObject,
		ResourceState beforeState,
		ResourceState afterState) = 0;
	// TO DO : Migrate render-target view binding to unified descriptor/view binding path.
	virtual void setRenderTarget(RenderTargetView* renderTargetView) = 0;
	// TO DO : Migrate clear operation to the same unified descriptor/view binding path.
	virtual void clearRenderTarget(
		RenderTargetView* renderTargetView,
		float red,
		float green,
		float blue,
		float alpha) = 0;
	virtual void close() = 0;
};
