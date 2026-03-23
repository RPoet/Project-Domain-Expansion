#pragma once

#include "Engine/Framework/Component.h"

class UpdateOrderRecordingComponent : public Component
{
public:
	UpdateOrderRecordingComponent(
		vector<uint32>* updateOrderStorage,
		uint32* tickCounter,
		uint32 updateOrderValue)
		: updateOrderStorage(updateOrderStorage)
		, tickCounter(tickCounter)
		, updateOrderValue(updateOrderValue)
	{
	}

	void tick(float deltaTimeSeconds) override
	{
		unused(deltaTimeSeconds);

		if (tickCounter != nullptr)
		{
			++(*tickCounter);
		}

		if (updateOrderStorage != nullptr)
		{
			updateOrderStorage->push_back(updateOrderValue);
		}
	}

private:
	vector<uint32>* updateOrderStorage = nullptr;
	uint32* tickCounter = nullptr;
	uint32 updateOrderValue = 0;
};

