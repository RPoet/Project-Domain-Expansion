#pragma once

#include "Engine/Module/Module.h"

class ReplayModule final : public StaticModule<ReplayModule>
{
public:
	enum class FrameworkExecutionCode : int32
	{
		saveActiveWorldFailed = -100,
	};

	enum class EditorExecutionCode : int32
	{
		invalidArguments = -300,
		frameworkMissing = -301,
		activeWorldMissing = -302,
		worldCreateFailed = -303,
		worldLoadFailed = -304,
		worldSaveFailed = -305,
		entityNotFound = -306,
		parentEntityNotFound = -307,
		entityCreateFailed = -308,
		entityReparentFailed = -309,
		entityDeleteFailed = -310,
		invalidEntityType = -311,
		componentNotFound = -312,
		componentAddFailed = -313,
		invalidComponentType = -314,
		meshAssetLoadFailed = -315,
		transformTargetInvalid = -316,
		deassetReadFailed = -317,
		deassetWriteFailed = -318,
		assetPathMismatch = -319,
	};

	ReplayModule()
		: StaticModule("ReplayModule")
	{
		registerReplayCommands();
	}

	bool initialize(Framework& framework) override;
	void preUpdate() override;
	void postUpdate() override;
	void shutdown() override;
	Framework* getFrameworkReference() const;

private:
	void registerReplayCommands();

	Framework* frameworkReference = nullptr;
};
