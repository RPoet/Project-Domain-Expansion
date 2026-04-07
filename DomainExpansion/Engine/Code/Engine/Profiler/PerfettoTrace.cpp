#include "Engine/Profiler/PerfettoTrace.h"

#include "Engine/Common/FileStream.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

PERFETTO_TRACK_EVENT_STATIC_STORAGE_IN_NAMESPACE(DomainExpansionPerfetto);

struct PerfettoCapture::State
{
	unique_pointer<perfetto::TracingSession> tracingSession = nullptr;
};

PerfettoCapture::~PerfettoCapture()
{
	delete state;
	state = nullptr;
}

void PerfettoCapture::initialize()
{
	static const bool initialized = []() -> bool
	{
		perfetto::TracingInitArgs tracingInitArgs = {};
		tracingInitArgs.backends |= perfetto::kInProcessBackend;
		perfetto::Tracing::Initialize(tracingInitArgs);
		DomainExpansionPerfetto::TrackEvent::Register();
		return true;
	}();

	unused(initialized);
}

string PerfettoCapture::buildDefaultStartupOutputFilePath()
{
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[PerfettoCapture][Assert] reason=disk_loader_module_missing");
	string defaultOutputFilePath = {};
	const bool resolvedOutputFilePath = diskLoaderModule->resolveAbsolutePathFromResources("Profile/engine_editor_startup_latest.pftrace", defaultOutputFilePath);
	assert(resolvedOutputFilePath && "[PerfettoCapture][Assert] reason=default_output_path_resolve_failed");
	return defaultOutputFilePath;
}

void PerfettoCapture::begin(const PerfettoCaptureOptions& captureOptions)
{
	assert(!isActive() && "[PerfettoCapture][Assert] reason=capture_already_active");
	assert(captureOptions.stopAfterFrameCount > 0 && "[PerfettoCapture][Assert] reason=stop_after_frame_count_invalid");

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[PerfettoCapture][Assert] reason=disk_loader_module_missing");
	string resolvedOutputFilePath = {};
	const bool resolvedOutputFilePathFromResources = diskLoaderModule->resolveAbsolutePathFromResources(captureOptions.outputFilePath, resolvedOutputFilePath);
	assert(resolvedOutputFilePathFromResources && "[PerfettoCapture][Assert] reason=output_path_resolve_failed");
	outputFilePath = moveValue(resolvedOutputFilePath);

	initialize();

	perfetto::TraceConfig traceConfig = {};
	traceConfig.add_buffers()->set_size_kb(128u * 1024u);
	auto* dataSourceConfig = traceConfig.add_data_sources()->mutable_config();
	dataSourceConfig->set_name("track_event");

	perfetto::protos::gen::TrackEventConfig trackEventConfig = {};
	trackEventConfig.add_disabled_categories("*");
	trackEventConfig.add_enabled_categories("startup");
	trackEventConfig.add_enabled_categories("framework");
	trackEventConfig.add_enabled_categories("world_load");
	trackEventConfig.add_enabled_categories("asset");
	trackEventConfig.add_enabled_categories("mesh");
	trackEventConfig.add_enabled_categories("xml");
	trackEventConfig.add_enabled_categories("disk");
	trackEventConfig.add_enabled_categories("render");
	dataSourceConfig->set_track_event_config_raw(trackEventConfig.SerializeAsString());

	delete state;
	state = new State();
	state->tracingSession = perfetto::Tracing::NewTrace();
	assert(state->tracingSession != nullptr && "[PerfettoCapture][Assert] reason=trace_session_create_failed");

	state->tracingSession->Setup(traceConfig);
	state->tracingSession->StartBlocking();
	remainingFrameCount = captureOptions.stopAfterFrameCount;
	output << "[Perfetto] capture_begin path=" << outputFilePath
		   << " stopAfterFrames=" << remainingFrameCount
		   << lineBreak;
}

void PerfettoCapture::beginStartupCapture(const PerfettoStartupCaptureRequest& startupCaptureRequest)
{
	if (!startupCaptureRequest.enabled)
	{
		return;
	}

	PerfettoCaptureOptions captureOptions{
		.outputFilePath = !startupCaptureRequest.outputFilePath.empty()
			? startupCaptureRequest.outputFilePath
			: buildDefaultStartupOutputFilePath(),
		.stopAfterFrameCount = startupCaptureRequest.stopAfterFrameCount
	};
	begin(captureOptions);
}

void PerfettoCapture::end()
{
	assert(isActive() && "[PerfettoCapture][Assert] reason=capture_not_active");
	assert(!outputFilePath.empty() && "[PerfettoCapture][Assert] reason=output_path_missing");

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[PerfettoCapture][Assert] reason=disk_loader_module_missing");

	state->tracingSession->StopBlocking();
	const auto traceData = state->tracingSession->ReadTraceBlocking();

	OutputFileStream fileStream = diskLoaderModule->openOutputFileStream(outputFilePath, true, true);

	if (!traceData.empty())
	{
		fileStream.write(traceData.data(), static_cast<stream_size>(traceData.size()));
	}

	fileStream.flush();
	fileStream.close();
	const bool savedTraceFile = !fileStream.fail();
	assert(savedTraceFile && "[PerfettoCapture][Assert] reason=trace_write_failed");
	delete state;
	state = nullptr;
	remainingFrameCount = 0;

	output << "[Perfetto] capture_saved path=" << outputFilePath
		   << " bytes=" << traceData.size()
		   << lineBreak;
}

void PerfettoCapture::endIfActive()
{
	if (!isActive())
	{
		return;
	}

	end();
}

void PerfettoCapture::onFramePresented()
{
	assert(isActive() && "[PerfettoCapture][Assert] reason=capture_not_active");
	assert(remainingFrameCount > 0 && "[PerfettoCapture][Assert] reason=frame_budget_missing");
	--remainingFrameCount;
}

void PerfettoCapture::onFramePresentedAndStopIfNeeded()
{
	if (!isActive())
	{
		return;
	}

	onFramePresented();
	if (shouldStopAfterPresentedFrame())
	{
		end();
	}
}

bool PerfettoCapture::shouldStopAfterPresentedFrame() const
{
	return isActive() && remainingFrameCount == 0;
}

bool PerfettoCapture::isActive() const
{
	return state != nullptr && state->tracingSession != nullptr;
}

const string& PerfettoCapture::getOutputFilePath() const
{
	return outputFilePath;
}
