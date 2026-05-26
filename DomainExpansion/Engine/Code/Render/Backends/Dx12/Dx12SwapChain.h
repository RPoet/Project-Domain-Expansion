#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include "Render/Backends/SwapChain.h"
#include "Render/Backends/ResourceObject.h"

class RenderBackend;

class Dx12SwapChain final : public SwapChain
{
public:
	Dx12SwapChain() = default;

	bool initialize(RenderBackend& renderBackend, uint32 width, uint32 height);
	bool resize(uint32 width, uint32 height) override;
	void shutdown();
	TextureResourceObject* getBackBufferResource(uint32 imageIndex);
	TextureResourceObject* getCurrentBackBufferResource() override;
	uint32 getFrameBufferCount() const;

	bool isRenderable() const override;
	uint32 getWidth() const override;
	uint32 getHeight() const override;
	uint32 getCurrentImageIndex() const override;
	void present() override;

private:
	static constexpr uint32 defaultFrameBufferCount = 2;

	bool createBackBufferResources();
	void releaseBackBufferResources();

	HandleWindow windowHandle = nullptr;
	com_pointer<IDXGISwapChain4> swapChain;
	vector<unique_pointer<TextureResourceObject>> backBufferResources;
	uint32 frameBufferCount = 0;
	uint32 backBufferWidth = 0;
	uint32 backBufferHeight = 0;
};
