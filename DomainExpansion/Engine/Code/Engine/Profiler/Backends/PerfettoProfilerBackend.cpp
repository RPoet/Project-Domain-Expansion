#include "Engine/Profiler/Backends/PerfettoProfilerBackend.h"

#include "Engine/Module/Timer/Timer.h"
#include "Engine/Profiler/ProfilerScopeHooks.h"
#include "Thread/Thread.h"

namespace
{
	uint64 buildPerfettoMicrosecondsSince(const double originTimeSeconds, const double valueTimeSeconds)
	{
		return static_cast<uint64>((valueTimeSeconds - originTimeSeconds) * 1000000.0);
	}

	bool ensurePerfettoOutputDirectory(const string& filePath)
	{
		const filesystem_path parentPath = filesystem_path(filePath).parent_path();
		if (parentPath.empty())
		{
			return true;
		}

		error_code createDirectoryError = {};
		create_directories(parentPath, createDirectoryError);
		return !createDirectoryError;
	}

	string escapePerfettoJSONString(const string_view text)
	{
		string escapedText = {};
		escapedText.reserve(text.length() + 8);
		for (size_t characterIndex = 0; characterIndex < text.length(); ++characterIndex)
		{
			const char character = text[characterIndex];
			switch (character)
			{
			case '\\':
				escapedText += "\\\\";
				break;
			case '"':
				escapedText += "\\\"";
				break;
			case '\n':
				escapedText += "\\n";
				break;
			case '\r':
				escapedText += "\\r";
				break;
			case '\t':
				escapedText += "\\t";
				break;
			default:
				escapedText.push_back(character);
				break;
			}
		}

		return escapedText;
	}

	uint64 getPerfettoCurrentThreadId()
	{
		return static_cast<uint64>(GetCurrentThreadId());
	}

	string buildPerfettoCurrentThreadName(const uint64 threadId)
	{
		const ThreadContext* currentThreadContext = getCurrentThreadContextConst();
		if (currentThreadContext != nullptr && !currentThreadContext->name.empty())
		{
			return currentThreadContext->name;
		}

		return "Thread" + to_string(threadId);
	}
}

struct PerfettoProfilerBackend::ThreadCaptureState
{
	PerfettoProfilerBackend* owner = nullptr;
	uint64 captureGeneration = 0;
	ThreadCaptureState* next = nullptr;
	vector<ActiveTraceEvent> activeEvents = {};
	vector<CompletedTraceEvent> completedEvents = {};
	vector<ProfilerXMLDocumentLoad> xmlDocumentLoads = {};
	string threadName = {};
	uint64 threadId = 0;

	void clearCaptureData()
	{
		activeEvents.clear();
		completedEvents.clear();
		xmlDocumentLoads.clear();
		threadName.clear();
		threadId = 0;
		next = nullptr;
	}
};

bool PerfettoProfilerBackend::isAvailable()
{
	return true;
}

bool PerfettoProfilerBackend::beginCapture(const ProfilerCaptureOptions& captureOptions)
{
	ProfilerScopeHookSuppressionGuard hookSuppressionGuard = {};
	if (!isCreated() || captureActive.load() || captureOptions.outputFilePath.empty())
	{
		return false;
	}

	resetCaptureState();
	activeCaptureOptions = captureOptions;
	captureGeneration.fetch_add(1);
	captureStartTimeSeconds.store(Timer::getCurrentTimeSeconds());
	captureActive.store(true);
	beginEvent("startup", "startup_capture", activeCaptureOptions.captureName);
	return true;
}

bool PerfettoProfilerBackend::endCapture(ProfilerCaptureResult& outCaptureResult)
{
	ProfilerScopeHookSuppressionGuard hookSuppressionGuard = {};
	outCaptureResult = {};
	if (!captureActive.load())
	{
		return false;
	}

	captureActive.store(false);
	waitForThreadCaptureStatesIdle();

	const double captureEndTimeSeconds = Timer::getCurrentTimeSeconds();
	mergeThreadCaptureStates(captureEndTimeSeconds);

	const bool wroteTraceFile = writeTraceFile(activeCaptureOptions.outputFilePath);
	string xmlSummaryFilePath = {};
	const bool wroteXMLSummaryFile = writeXMLSummaryFile(activeCaptureOptions.outputFilePath, xmlSummaryFilePath);

	outCaptureResult.captureName = activeCaptureOptions.captureName;
	outCaptureResult.outputFilePath = wroteTraceFile ? activeCaptureOptions.outputFilePath : "";
	outCaptureResult.xmlSummaryFilePath = wroteXMLSummaryFile ? xmlSummaryFilePath : "";
	resetCaptureState();
	return wroteTraceFile;
}

bool PerfettoProfilerBackend::isCaptureActive() const
{
	return captureActive.load();
}

PerfettoProfilerBackend::ThreadCaptureState& PerfettoProfilerBackend::acquireThreadCaptureState()
{
	static thread_local ThreadCaptureState threadCaptureState = {};
	ThreadCaptureState& captureState = threadCaptureState;
	const uint64 currentCaptureGeneration = captureGeneration.load();
	if (captureState.owner == this && captureState.captureGeneration == currentCaptureGeneration)
	{
		return captureState;
	}

	captureState.clearCaptureData();
	captureState.owner = this;
	captureState.captureGeneration = currentCaptureGeneration;
	captureState.threadId = getPerfettoCurrentThreadId();
	captureState.threadName = buildPerfettoCurrentThreadName(captureState.threadId);
	registerThreadCaptureState(captureState);
	return captureState;
}

void PerfettoProfilerBackend::registerThreadCaptureState(ThreadCaptureState& threadCaptureState)
{
	ThreadCaptureState* currentHead = captureThreadStateHead.load();
	do
	{
		threadCaptureState.next = currentHead;
	}
	while (!captureThreadStateHead.compare_exchange_weak(currentHead, &threadCaptureState));
}

void PerfettoProfilerBackend::beginEvent(const char* category, const char* name, const string& detail)
{
	ProfilerScopeHookSuppressionGuard hookSuppressionGuard = {};
	activeOperationCount.fetch_add(1);
	if (!captureActive.load() || category == nullptr || name == nullptr)
	{
		activeOperationCount.fetch_sub(1);
		return;
	}

	ThreadCaptureState& threadCaptureState = acquireThreadCaptureState();
	threadCaptureState.activeEvents.push_back({
		.category = category,
		.name = name,
		.detail = detail,
		.beginTimeSeconds = Timer::getCurrentTimeSeconds(),
	});
	activeOperationCount.fetch_sub(1);
}

void PerfettoProfilerBackend::endEvent()
{
	ProfilerScopeHookSuppressionGuard hookSuppressionGuard = {};
	activeOperationCount.fetch_add(1);
	if (!captureActive.load())
	{
		activeOperationCount.fetch_sub(1);
		return;
	}

	ThreadCaptureState& threadCaptureState = acquireThreadCaptureState();
	if (!threadCaptureState.activeEvents.empty())
	{
		const double endTimeSeconds = Timer::getCurrentTimeSeconds();
		const double captureStartSeconds = captureStartTimeSeconds.load();
		ActiveTraceEvent activeEvent = moveValue(threadCaptureState.activeEvents.back());
		threadCaptureState.activeEvents.pop_back();
		threadCaptureState.completedEvents.push_back({
			.category = activeEvent.category,
			.name = activeEvent.name,
			.detail = moveValue(activeEvent.detail),
			.threadId = threadCaptureState.threadId,
			.beginMicroseconds = buildPerfettoMicrosecondsSince(captureStartSeconds, activeEvent.beginTimeSeconds),
			.durationMicroseconds = buildPerfettoMicrosecondsSince(activeEvent.beginTimeSeconds, endTimeSeconds),
		});
	}
	activeOperationCount.fetch_sub(1);
}

void PerfettoProfilerBackend::recordXMLDocumentLoad(const ProfilerXMLDocumentLoad& documentLoad)
{
	ProfilerScopeHookSuppressionGuard hookSuppressionGuard = {};
	activeOperationCount.fetch_add(1);
	if (!captureActive.load())
	{
		activeOperationCount.fetch_sub(1);
		return;
	}

	ThreadCaptureState& threadCaptureState = acquireThreadCaptureState();
	threadCaptureState.xmlDocumentLoads.push_back(documentLoad);
	activeOperationCount.fetch_sub(1);
}

bool PerfettoProfilerBackend::createBackendState()
{
	resetCaptureState();
	return true;
}

void PerfettoProfilerBackend::destroyBackendState()
{
	resetCaptureState();
}

void PerfettoProfilerBackend::waitForThreadCaptureStatesIdle() const
{
	while (activeOperationCount.load() != 0)
	{
		yieldCurrentThreadExecution();
	}
}

void PerfettoProfilerBackend::mergeThreadCaptureStates(const double captureEndTimeSeconds)
{
	const double captureStartSeconds = captureStartTimeSeconds.load();
	const uint64 currentCaptureGeneration = captureGeneration.load();
	completedEvents.clear();
	threadNameById.clear();
	xmlDocumentLoads.clear();

	for (ThreadCaptureState* threadCaptureState = captureThreadStateHead.load(); threadCaptureState != nullptr;)
	{
		ThreadCaptureState* nextThreadCaptureState = threadCaptureState->next;
		if (threadCaptureState->owner != this || threadCaptureState->captureGeneration != currentCaptureGeneration)
		{
			threadCaptureState = nextThreadCaptureState;
			continue;
		}

		threadNameById[threadCaptureState->threadId] = threadCaptureState->threadName;

		while (!threadCaptureState->activeEvents.empty())
		{
			ActiveTraceEvent activeEvent = moveValue(threadCaptureState->activeEvents.back());
			threadCaptureState->activeEvents.pop_back();
			threadCaptureState->completedEvents.push_back({
				.category = activeEvent.category,
				.name = activeEvent.name,
				.detail = moveValue(activeEvent.detail),
				.threadId = threadCaptureState->threadId,
				.beginMicroseconds = buildPerfettoMicrosecondsSince(captureStartSeconds, activeEvent.beginTimeSeconds),
				.durationMicroseconds = buildPerfettoMicrosecondsSince(activeEvent.beginTimeSeconds, captureEndTimeSeconds),
			});
		}

		for (uint32 eventIndex = 0; eventIndex < static_cast<uint32>(threadCaptureState->completedEvents.size()); ++eventIndex)
		{
			completedEvents.push_back(moveValue(threadCaptureState->completedEvents[eventIndex]));
		}

		for (uint32 documentIndex = 0; documentIndex < static_cast<uint32>(threadCaptureState->xmlDocumentLoads.size()); ++documentIndex)
		{
			xmlDocumentLoads.push_back(moveValue(threadCaptureState->xmlDocumentLoads[documentIndex]));
		}

		threadCaptureState->clearCaptureData();
		threadCaptureState->owner = this;
		threadCaptureState->captureGeneration = currentCaptureGeneration;
		threadCaptureState = nextThreadCaptureState;
	}
}

bool PerfettoProfilerBackend::writeTraceFile(const string& outputFilePath) const
{
	if (outputFilePath.empty() || !ensurePerfettoOutputDirectory(outputFilePath))
	{
		return false;
	}

	vector<CompletedTraceEvent> completedEventsSnapshot = completedEvents;
	unordered_map<uint64, string> threadNameByIdSnapshot = threadNameById;

	sort(
		completedEventsSnapshot.begin(),
		completedEventsSnapshot.end(),
		[](const CompletedTraceEvent& left, const CompletedTraceEvent& right)
		{
			if (left.beginMicroseconds == right.beginMicroseconds)
			{
				return left.threadId < right.threadId;
			}

			return left.beginMicroseconds < right.beginMicroseconds;
		});

	vector<pair<uint64, string>> threadMetadata = {};
	threadMetadata.reserve(threadNameByIdSnapshot.size());
	for (auto threadNameIterator = threadNameByIdSnapshot.begin();
		threadNameIterator != threadNameByIdSnapshot.end();
		++threadNameIterator)
	{
		threadMetadata.push_back({ threadNameIterator->first, threadNameIterator->second });
	}

	sort(
		threadMetadata.begin(),
		threadMetadata.end(),
		[](const pair<uint64, string>& left, const pair<uint64, string>& right)
		{
			return left.first < right.first;
		});

	output_file_stream fileStream(outputFilePath, output_file_stream::out | output_file_stream::trunc);
	if (!fileStream.is_open() || !fileStream.good())
	{
		return false;
	}

	fileStream << "{\"traceEvents\":[";
	fileStream << "{\"name\":\"process_name\",\"ph\":\"M\",\"pid\":1,\"tid\":0,\"args\":{\"name\":\"DomainExpansion Engine\"}}";

	for (uint32 threadIndex = 0; threadIndex < static_cast<uint32>(threadMetadata.size()); ++threadIndex)
	{
		const pair<uint64, string>& threadEntry = threadMetadata[threadIndex];
		fileStream << ",{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":1,\"tid\":" << threadEntry.first
				   << ",\"args\":{\"name\":\"" << escapePerfettoJSONString(threadEntry.second) << "\"}}";
	}

	for (uint32 eventIndex = 0; eventIndex < static_cast<uint32>(completedEventsSnapshot.size()); ++eventIndex)
	{
		const CompletedTraceEvent& completedEvent = completedEventsSnapshot[eventIndex];
		fileStream << ",{\"name\":\""
				   << escapePerfettoJSONString(completedEvent.name != nullptr ? string_view(completedEvent.name) : string_view())
				   << "\",\"cat\":\""
				   << escapePerfettoJSONString(completedEvent.category != nullptr ? string_view(completedEvent.category) : string_view())
				   << "\",\"ph\":\"X\",\"ts\":" << completedEvent.beginMicroseconds
				   << ",\"dur\":" << completedEvent.durationMicroseconds
				   << ",\"pid\":1,\"tid\":" << completedEvent.threadId;
		if (!completedEvent.detail.empty())
		{
			fileStream << ",\"args\":{\"detail\":\"" << escapePerfettoJSONString(completedEvent.detail) << "\"}";
		}

		fileStream << "}";
	}

	fileStream << "]}";
	return fileStream.good();
}

bool PerfettoProfilerBackend::writeXMLSummaryFile(const string& outputFilePath, string& outSummaryFilePath) const
{
	outSummaryFilePath.clear();

	const filesystem_path traceOutputPath(outputFilePath);
	const string summaryFileName = traceOutputPath.stem().string() + ".xml-summary.txt";
	const filesystem_path summaryFilePath = traceOutputPath.has_parent_path()
		? (traceOutputPath.parent_path() / summaryFileName)
		: filesystem_path(summaryFileName);
	const string summaryPathText = summaryFilePath.lexically_normal().string();
	if (!ensurePerfettoOutputDirectory(summaryPathText))
	{
		return false;
	}

	const vector<ProfilerXMLDocumentLoad> xmlDocumentLoadsSnapshot = xmlDocumentLoads;
	const string captureName = activeCaptureOptions.captureName;

	struct XMLLoadSummaryEntry
	{
		string filePath = {};
		uint64 callCount = 0;
		uint64 totalBytes = 0;
		uint64 totalKeys = 0;
		uint64 failedCallCount = 0;
		double totalReadMilliseconds = 0.0;
		double totalParseMilliseconds = 0.0;
	};

	vector<XMLLoadSummaryEntry> summaryEntries = {};
	unordered_map<string, uint32> summaryEntryIndexByPath = {};
	double totalReadMilliseconds = 0.0;
	double totalParseMilliseconds = 0.0;
	uint64 totalBytes = 0;
	uint64 totalKeys = 0;

	for (uint32 loadIndex = 0; loadIndex < static_cast<uint32>(xmlDocumentLoadsSnapshot.size()); ++loadIndex)
	{
		const ProfilerXMLDocumentLoad& documentLoad = xmlDocumentLoadsSnapshot[loadIndex];
		totalReadMilliseconds += documentLoad.readMilliseconds;
		totalParseMilliseconds += documentLoad.parseMilliseconds;
		totalBytes += documentLoad.fileSizeBytes;
		totalKeys += documentLoad.keyCount;

		auto foundSummaryIndex = summaryEntryIndexByPath.find(documentLoad.filePath);
		if (foundSummaryIndex == summaryEntryIndexByPath.end())
		{
			XMLLoadSummaryEntry newEntry = {};
			newEntry.filePath = documentLoad.filePath;
			summaryEntryIndexByPath.emplace(documentLoad.filePath, static_cast<uint32>(summaryEntries.size()));
			summaryEntries.push_back(moveValue(newEntry));
			foundSummaryIndex = summaryEntryIndexByPath.find(documentLoad.filePath);
		}

		XMLLoadSummaryEntry& summaryEntry = summaryEntries[foundSummaryIndex->second];
		++summaryEntry.callCount;
		summaryEntry.totalBytes += documentLoad.fileSizeBytes;
		summaryEntry.totalKeys += documentLoad.keyCount;
		summaryEntry.totalReadMilliseconds += documentLoad.readMilliseconds;
		summaryEntry.totalParseMilliseconds += documentLoad.parseMilliseconds;
		if (documentLoad.parseResult != "succeeded")
		{
			++summaryEntry.failedCallCount;
		}
	}

	sort(
		summaryEntries.begin(),
		summaryEntries.end(),
		[](const XMLLoadSummaryEntry& left, const XMLLoadSummaryEntry& right)
		{
			return (left.totalReadMilliseconds + left.totalParseMilliseconds)
				> (right.totalReadMilliseconds + right.totalParseMilliseconds);
		});

	output_file_stream fileStream(summaryPathText, output_file_stream::out | output_file_stream::trunc);
	if (!fileStream.is_open() || !fileStream.good())
	{
		return false;
	}

	fileStream << ("capture=" + captureName) << lineBreak;
	fileStream << ("documents=" + to_string(xmlDocumentLoadsSnapshot.size())) << lineBreak;
	fileStream << ("uniqueFiles=" + to_string(summaryEntries.size())) << lineBreak;
	fileStream << ("totalBytes=" + to_string(totalBytes)) << lineBreak;
	fileStream << ("totalKeys=" + to_string(totalKeys)) << lineBreak;
	fileStream << ("totalReadMs=" + to_string(totalReadMilliseconds)) << lineBreak;
	fileStream << ("totalParseMs=" + to_string(totalParseMilliseconds)) << lineBreak;
	fileStream << ("totalXmlMs=" + to_string(totalReadMilliseconds + totalParseMilliseconds)) << lineBreak;
	fileStream << lineBreak;
	fileStream << "[Top XML Files]" << lineBreak;

	const uint32 maxSummaryCount = static_cast<uint32>(summaryEntries.size() < 20 ? summaryEntries.size() : 20);
	for (uint32 summaryIndex = 0; summaryIndex < maxSummaryCount; ++summaryIndex)
	{
		const XMLLoadSummaryEntry& summaryEntry = summaryEntries[summaryIndex];
		const double totalMilliseconds = summaryEntry.totalReadMilliseconds + summaryEntry.totalParseMilliseconds;
		const double averageMilliseconds = summaryEntry.callCount > 0
			? totalMilliseconds / static_cast<double>(summaryEntry.callCount)
			: 0.0;
		fileStream << to_string(summaryIndex + 1)
				   << ". totalMs=" << to_string(totalMilliseconds)
				   << " avgMs=" << to_string(averageMilliseconds)
				   << " readMs=" << to_string(summaryEntry.totalReadMilliseconds)
				   << " parseMs=" << to_string(summaryEntry.totalParseMilliseconds)
				   << " calls=" << to_string(summaryEntry.callCount)
				   << " bytes=" << to_string(summaryEntry.totalBytes)
				   << " keys=" << to_string(summaryEntry.totalKeys)
				   << " failedCalls=" << to_string(summaryEntry.failedCallCount)
				   << " file=" << summaryEntry.filePath
				   << lineBreak;
	}

	if (!xmlDocumentLoadsSnapshot.empty())
	{
		fileStream << lineBreak;
		fileStream << "[Raw XML Loads]" << lineBreak;
		for (uint32 loadIndex = 0; loadIndex < static_cast<uint32>(xmlDocumentLoadsSnapshot.size()); ++loadIndex)
		{
			const ProfilerXMLDocumentLoad& documentLoad = xmlDocumentLoadsSnapshot[loadIndex];
			fileStream << to_string(loadIndex + 1)
					   << ". totalMs=" << to_string(documentLoad.readMilliseconds + documentLoad.parseMilliseconds)
					   << " readMs=" << to_string(documentLoad.readMilliseconds)
					   << " parseMs=" << to_string(documentLoad.parseMilliseconds)
					   << " bytes=" << to_string(documentLoad.fileSizeBytes)
					   << " keys=" << to_string(documentLoad.keyCount)
					   << " result=" << documentLoad.parseResult
					   << " file=" << documentLoad.filePath
					   << lineBreak;
		}
	}

	outSummaryFilePath = summaryPathText;
	return fileStream.good();
}

void PerfettoProfilerBackend::resetCaptureState()
{
	captureActive.store(false);
	captureStartTimeSeconds.store(0.0);
	captureThreadStateHead.store(nullptr);
	activeOperationCount.store(0);
	activeCaptureOptions = {};
	completedEvents.clear();
	threadNameById.clear();
	xmlDocumentLoads.clear();
}
