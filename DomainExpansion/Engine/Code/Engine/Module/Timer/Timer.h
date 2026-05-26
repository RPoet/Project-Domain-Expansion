#pragma once
#include "Engine/Module/Module.h"

class Timer final : public StaticModule<Timer>
{
public:
	Timer()
		: StaticModule("Timer")
	{}

	bool initialize(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	static double getCurrentTimeSeconds();
	double getDeltaTime() const { return delta; }
	double getTime() const { return time; }

private:
	double delta{ 0.0 };
	double prevTime{ 0.0 };
	double time{ 0.0 };
};

class Stopwatch final
{
public:
	Stopwatch();
	void restart();
	double getElapsedSeconds() const;
	float getElapsedMilliseconds() const;

private:
	double startTimeSeconds = 0.0;
};

class ScopedTimer final
{
public:
	explicit ScopedTimer(float& outElapsedMilliseconds);
	~ScopedTimer();

private:
	Stopwatch stopwatch = {};
	float& outElapsedMilliseconds;
};
