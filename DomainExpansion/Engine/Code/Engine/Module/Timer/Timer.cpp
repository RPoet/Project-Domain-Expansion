#include "Engine/Module/Timer/Timer.h"

static double getCurrentTimeSeconds()
{
	return duration_seconds(steady_clock::now().time_since_epoch()).count();
}

bool Timer::init(Framework& framework)
{
	unused(framework);
	delta = 0.0;
	time = 0.0;
	prevTime = getCurrentTimeSeconds();
	return true;
}

void Timer::preUpdate()
{
	const double currentTime = getCurrentTimeSeconds();
	double frameDelta = currentTime - prevTime;
	if (frameDelta < 0.0)
	{
		frameDelta = 0.0;
	}

	static constexpr double maximumFrameDeltaSeconds = 0.25;
	if (frameDelta > maximumFrameDeltaSeconds)
	{
		frameDelta = maximumFrameDeltaSeconds;
	}

	delta = frameDelta;
	time += frameDelta;
	prevTime = currentTime;
}

void Timer::postUpdate()
{
}

void Timer::shutdown()
{
	delta = 0.0;
	time = 0.0;
	prevTime = 0.0;
}
