#include "Engine/Profiler/Backends/PerfettoProfilerBackend.h"

#include "Engine/Module/Timer/Timer.h"

static uint64 buildPerfettoMicrosecondsSince(const double originTimeSeconds, const double valueTimeSeconds)
{
	return static_cast<uint64>((valueTimeSeconds - originTimeSeconds) * 1000000.0);
}

static bool ensurePerfettoOutputDirectory(const string& filePath)
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

static string escapePerfettoJSONString(const string& text)
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

bool PerfettoProfilerBackend::isAvailable()
{
	return true;
}

bool PerfettoProfilerBackend::beginCapture(const ProfilerCaptureOptions& captureOptions)
{
	if (!isCreated() || captureActive || captureOptions.outputFilePath.empty())
	{
		return false;
	}

	resetCaptureState();
	activeCaptureOptions = captureOptions;
	captureStartTimeSeconds = Timer::getCurrentTimeSeconds();
	captureActive = true;
	beginEvent("startup", "startup_capture", activeCaptureOptions.captureName);
	return true;
}

bool PerfettoProfilerBackend::endCapture(ProfilerCaptureResult& outCaptureResult)
{
	outCaptureResult = {};
	if (!captureActive)
	{
		return false;
	}

	while (!activeEvents.empty())
	{
		endEvent();
	}

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
	return captureActive;
}

void PerfettoProfilerBackend::beginEvent(const char* category, const char* name, const string& detail)
{
	if (!captureActive || category == nullptr || name == nullptr)
	{
		return;
	}

	ActiveTraceEvent traceEvent = {
		.category = category,
		.name = name,
		.detail = detail,
		.beginTimeSeconds = Timer::getCurrentTimeSeconds(),
	};
	activeEvents.push_back(moveValue(traceEvent));
}

void PerfettoProfilerBackend::endEvent()
{
	if (!captureActive || activeEvents.empty())
	{
		return;
	}

	const double endTimeSeconds = Timer::getCurrentTimeSeconds();
	ActiveTraceEvent activeEvent = moveValue(activeEvents.back());
	activeEvents.pop_back();

	CompletedTraceEvent completedEvent = {
		.category = moveValue(activeEvent.category),
		.name = moveValue(activeEvent.name),
		.detail = moveValue(activeEvent.detail),
		.beginMicroseconds = buildPerfettoMicrosecondsSince(captureStartTimeSeconds, activeEvent.beginTimeSeconds),
		.durationMicroseconds = buildPerfettoMicrosecondsSince(activeEvent.beginTimeSeconds, endTimeSeconds),
	};
	completedEvents.push_back(moveValue(completedEvent));
}

void PerfettoProfilerBackend::recordXMLDocumentLoad(const ProfilerXMLDocumentLoad& documentLoad)
{
	if (!captureActive)
	{
		return;
	}

	xmlDocumentLoads.push_back(documentLoad);
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

bool PerfettoProfilerBackend::writeTraceFile(const string& outputFilePath) const
{
	if (outputFilePath.empty() || !ensurePerfettoOutputDirectory(outputFilePath))
	{
		return false;
	}

	output_file_stream fileStream(outputFilePath, output_file_stream::out | output_file_stream::trunc);
	if (!fileStream.is_open() || !fileStream.good())
	{
		return false;
	}

	fileStream << "{\"traceEvents\":[";
	fileStream << "{\"name\":\"process_name\",\"ph\":\"M\",\"pid\":1,\"tid\":1,\"args\":{\"name\":\"DomainExpansion Engine\"}},";
	fileStream << "{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":1,\"tid\":1,\"args\":{\"name\":\"MainThread\"}}";

	for (uint32 eventIndex = 0; eventIndex < static_cast<uint32>(completedEvents.size()); ++eventIndex)
	{
		const CompletedTraceEvent& completedEvent = completedEvents[eventIndex];
		fileStream << ",{\"name\":\"" << escapePerfettoJSONString(completedEvent.name)
				   << "\",\"cat\":\"" << escapePerfettoJSONString(completedEvent.category)
				   << "\",\"ph\":\"X\",\"ts\":" << completedEvent.beginMicroseconds
				   << ",\"dur\":" << completedEvent.durationMicroseconds
				   << ",\"pid\":1,\"tid\":1";
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

	for (uint32 loadIndex = 0; loadIndex < static_cast<uint32>(xmlDocumentLoads.size()); ++loadIndex)
	{
		const ProfilerXMLDocumentLoad& documentLoad = xmlDocumentLoads[loadIndex];
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

	fileStream << ("capture=" + activeCaptureOptions.captureName) << lineBreak;
	fileStream << ("documents=" + to_string(xmlDocumentLoads.size())) << lineBreak;
	fileStream << ("uniqueFiles=" + to_string(summaryEntries.size())) << lineBreak;
	fileStream << ("totalBytes=" + to_string(totalBytes)) << lineBreak;
	fileStream << ("totalKeys=" + to_string(totalKeys)) << lineBreak;
	fileStream << ("totalReadMs=" + to_string(totalReadMilliseconds)) << lineBreak;
	fileStream << ("totalParseMs=" + to_string(totalParseMilliseconds)) << lineBreak;
	fileStream << ("totalXmlMs=" + to_string(totalReadMilliseconds + totalParseMilliseconds)) << lineBreak;
	fileStream << lineBreak;
	fileStream << "[Top XML Files]" << lineBreak;

	const uint32 maxSummaryCount = static_cast<uint32>(std::min<size_t>(summaryEntries.size(), 20));
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

	if (!xmlDocumentLoads.empty())
	{
		fileStream << lineBreak;
		fileStream << "[Raw XML Loads]" << lineBreak;
		for (uint32 loadIndex = 0; loadIndex < static_cast<uint32>(xmlDocumentLoads.size()); ++loadIndex)
		{
			const ProfilerXMLDocumentLoad& documentLoad = xmlDocumentLoads[loadIndex];
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
	captureActive = false;
	captureStartTimeSeconds = 0.0;
	activeCaptureOptions = {};
	activeEvents.clear();
	completedEvents.clear();
	xmlDocumentLoads.clear();
}
