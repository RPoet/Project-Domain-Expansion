#pragma once

#include "Engine/Module/Module.h"
#include "Render/Backends/RenderBackend.h"

class RenderBackendModule final : public StaticModule<RenderBackendModule>
{
public:
	RenderBackendModule()
		: StaticModule("RenderBackendModule")
	{}

	bool init(Framework& framework) override final;
	void update() override final;
	void shutdown() override final;

	bool createBackend(const RenderBackendCreateOptions& createOptions);
	void destroyBackend();
	bool resizeBackend(uint32 width, uint32 height);
	RenderBackend* getBackend();
	const RenderBackend* getBackend() const;
	bool isBackendCreated() const;

private:
	unique_pointer<RenderBackend> renderBackend;
};
