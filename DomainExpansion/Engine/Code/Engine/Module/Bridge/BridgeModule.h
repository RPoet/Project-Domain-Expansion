#pragma once

#include "Bridge/BaseBridge.h"
#include "Engine/Module/Module.h"

class BridgeModule final : public StaticModule<BridgeModule>
{
public:
	BridgeModule()
		: StaticModule("BridgeModule")
	{
	}

	bool init(Framework& framework) override;
	void preUpdate() override;
	void postUpdate() override;
	void shutdown() override;
	void flushPendingDeletes();

	void registerBridge(BaseBridge* bridge);
	void unregisterBridge(BaseBridge* bridge);

private:
	vector<BaseBridge*> bridgeStorage = {};
};
