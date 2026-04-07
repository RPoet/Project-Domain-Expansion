#pragma once

#include "Render/Backends/RenderBackend.h"

namespace FrameSyncCoordinator
{
inline void waitForBackendIdle(RenderBackend& renderBackend)
{
	auto* frameSync = renderBackend.getSyncObject();
	if (frameSync != nullptr)
	{
		frameSync->wait();
	}
}

inline bool hasBackendFrameSync(RenderBackend& renderBackend)
{
	return renderBackend.getSyncObject() != nullptr;
}

inline void waitForPreviousSubmission(RenderBackend& renderBackend, const uint64 lastSubmittedFrameSyncValue)
{
	if (lastSubmittedFrameSyncValue <= 1)
	{
		return;
	}

	auto* frameSync = renderBackend.getSyncObject();
	assert(frameSync != nullptr && "[FrameSyncCoordinator][Assert] reason=backend_frame_sync_missing");
	frameSync->wait(lastSubmittedFrameSyncValue - 1);
}

inline uint64 signalFrameSubmission(RenderBackend& renderBackend)
{
	auto* frameSync = renderBackend.getSyncObject();
	assert(frameSync != nullptr && "[FrameSyncCoordinator][Assert] reason=backend_frame_sync_missing");
	return frameSync->signal();
}
}
