#include "Engine/Module/Timer/Timer.h"

static double getMonotonicTimeSeconds()
{
	LARGE_INTEGER performanceCounterFrequency = {};
	const Bool resolvedFrequency = QueryPerformanceFrequency(&performanceCounterFrequency);
	assert(resolvedFrequency != FALSE && performanceCounterFrequency.QuadPart > 0 && "[Timer][Assert] reason=performance_counter_frequency_unavailable");

	LARGE_INTEGER performanceCounterValue = {};
	const Bool resolvedCounter = QueryPerformanceCounter(&performanceCounterValue);
	assert(resolvedCounter != FALSE && "[Timer][Assert] reason=performance_counter_read_failed");
	return static_cast<double>(performanceCounterValue.QuadPart) / static_cast<double>(performanceCounterFrequency.QuadPart);
}

double Timer::getCurrentTimeSeconds()
{
	return getMonotonicTimeSeconds();
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

	if (frameDelta > 0.25)
	{
		frameDelta = 0.25;
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
