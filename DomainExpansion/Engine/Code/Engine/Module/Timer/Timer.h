#pragma once
#include "Engine/Module/Module.h"

class Timer final : public StaticModule<Timer>
{
public:
	Timer()
		: StaticModule("Timer")
	{}

	bool init(Framework& framework) override final;
	void update() override final;
	void shutdown() override final;

	double getDeltaTime() const { return delta; }
	double getTime() const { return time; }

private:
	double delta{ 0.0 };
	double prevTime{ 0.0 };
	double time{ 0.0 };
};
