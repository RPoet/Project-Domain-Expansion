#include "Engine/Module/Timer/Timer.h"

double Timer::getCurrentTimeSeconds()
{
	return duration_seconds(steady_clock::now().time_since_epoch()).count();
}

bool Timer::initialize(Framework& framework)
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

Stopwatch::Stopwatch()
{
	restart();
}

void Stopwatch::restart()
{
	startTimeSeconds = Timer::getCurrentTimeSeconds();
}

double Stopwatch::getElapsedSeconds() const
{
	const double currentTimeSeconds = Timer::getCurrentTimeSeconds();
	return currentTimeSeconds >= startTimeSeconds ? currentTimeSeconds - startTimeSeconds : 0.0;
}

float Stopwatch::getElapsedMilliseconds() const
{
	return static_cast<float>(getElapsedSeconds() * 1000.0);
}

ScopedTimer::ScopedTimer(float& outElapsedMilliseconds)
	: outElapsedMilliseconds(outElapsedMilliseconds)
{
}

ScopedTimer::~ScopedTimer()
{
	outElapsedMilliseconds = stopwatch.getElapsedMilliseconds();
}
