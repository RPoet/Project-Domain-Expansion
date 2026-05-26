#include "Engine/Module/Profiler/ProfilerModule.h"

#include "Engine/Framework/Framework.h"

static ProfilerBackend* activeProfilerBackend = nullptr;

bool ProfilerModule::initialize(Framework& framework)
{
	const FrameworkProfilerOptions& profilerOptions = framework.getProfilerOptions();
	const bool createdBackend = createBackend({ .backendType = profilerOptions.backendType });
	assert(createdBackend && "[ProfilerModule][Assert] reason=backend_create_failed");

	if (!profilerOptions.startupCaptureOutputFilePath.empty())
	{
		const bool beganCapture = beginCapture({
			.captureName = "startup_initial_load",
			.outputFilePath = profilerOptions.startupCaptureOutputFilePath,
		});
		assert(beganCapture && "[ProfilerModule][Assert] reason=startup_capture_begin_failed");
	}

	return createdBackend;
}

void ProfilerModule::preUpdate()
{
	if (profilerBackend != nullptr)
	{
		profilerBackend->preUpdate();
	}
}

void ProfilerModule::postUpdate()
{
	if (profilerBackend != nullptr)
	{
		profilerBackend->postUpdate();
	}
}

void ProfilerModule::shutdown()
{
	if (isCaptureActive())
	{
		ProfilerCaptureResult captureResult = {};
		const bool endedCapture = endCapture(captureResult);
		assert(endedCapture && "[ProfilerModule][Assert] reason=shutdown_capture_end_failed");

		if (!captureResult.outputFilePath.empty())
		{
			output << "[Profiler][CaptureSaved] trace=" << captureResult.outputFilePath;
			if (!captureResult.xmlSummaryFilePath.empty())
			{
				output << " xmlSummary=" << captureResult.xmlSummaryFilePath;
			}

			output << lineBreak;
		}
	}

	destroyBackend();
}

bool ProfilerModule::createBackend(const ProfilerBackendCreateOptions& createOptions)
{
	destroyBackend();

	unique_pointer<ProfilerBackend> createdBackend = ProfilerBackend::createBackend(createOptions.backendType);
	if (createdBackend == nullptr || !createdBackend->create(createOptions))
	{
		return false;
	}

	profilerBackend = moveValue(createdBackend);
	return true;
}

void ProfilerModule::destroyBackend()
{
	activeProfilerBackend = nullptr;
	if (profilerBackend != nullptr)
	{
		profilerBackend->destroy();
		profilerBackend.reset();
	}
}

bool ProfilerModule::beginCapture(const ProfilerCaptureOptions& captureOptions)
{
	assert(profilerBackend != nullptr && "[ProfilerModule][Assert] reason=begin_capture_backend_missing");

	const bool beganCapture = profilerBackend->beginCapture(captureOptions);
	if (beganCapture)
	{
		activeProfilerBackend = profilerBackend.get();
	}

	return beganCapture;
}

bool ProfilerModule::endCapture(ProfilerCaptureResult& outCaptureResult)
{
	assert(profilerBackend != nullptr && "[ProfilerModule][Assert] reason=end_capture_backend_missing");

	const bool endedCapture = profilerBackend->endCapture(outCaptureResult);
	if (endedCapture)
	{
		activeProfilerBackend = nullptr;
	}

	return endedCapture;
}

bool ProfilerModule::isCaptureActive() const
{
	return profilerBackend == nullptr ? false : profilerBackend->isCaptureActive();
}

ProfilerBackend* ProfilerModule::getActiveBackendFast()
{
	return activeProfilerBackend;
}

ProfilerBackend* ProfilerModule::getBackend()
{
	return profilerBackend.get();
}

const ProfilerBackend* ProfilerModule::getBackend() const
{
	return profilerBackend.get();
}

bool ProfilerModule::isBackendCreated() const
{
	return profilerBackend != nullptr;
}

void ProfilerModule::recordXMLDocumentLoad(const ProfilerXMLDocumentLoad& documentLoad)
{
	if (profilerBackend != nullptr)
	{
		profilerBackend->recordXMLDocumentLoad(documentLoad);
	}
}
