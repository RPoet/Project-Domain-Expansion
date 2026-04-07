#include "Engine/Module/Profiler/ProfilerModule.h"

#include "Engine/Framework/Framework.h"

bool ProfilerModule::init(Framework& framework)
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
	if (profilerBackend != nullptr)
	{
		profilerBackend->destroy();
		profilerBackend.reset();
	}
}

bool ProfilerModule::beginCapture(const ProfilerCaptureOptions& captureOptions)
{
	return profilerBackend == nullptr ? false : profilerBackend->beginCapture(captureOptions);
}

bool ProfilerModule::endCapture(ProfilerCaptureResult& outCaptureResult)
{
	return profilerBackend == nullptr ? false : profilerBackend->endCapture(outCaptureResult);
}

bool ProfilerModule::isCaptureActive() const
{
	return profilerBackend == nullptr ? false : profilerBackend->isCaptureActive();
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
