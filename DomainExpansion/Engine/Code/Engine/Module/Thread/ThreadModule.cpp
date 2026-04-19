#include "Engine/Module/Thread/ThreadModule.h"

bool ThreadModule::init(Framework& framework)
{
	frameworkReference = &framework;
	acceptingJobs = false;
	stopRequested = false;
	nextLogicalThreadIndex = 1;
	nextJobSerial = 1;

	mainThreadContext.logicalThreadIndex = 0;
	mainThreadContext.nativeThreadId = static_cast<uint64>(GetCurrentThreadId());
	mainThreadContext.backendType = getDefaultThreadBackendType();
	mainThreadContext.role = ThreadRole::main;
	mainThreadContext.state = ThreadState::running;
	mainThreadContext.name = "MainThread";
	mainThreadContext.stopRequested.store(false);
	setCurrentThreadContext(&mainThreadContext);

	acceptingJobs = true;
	return createWorkerFleet();
}

void ThreadModule::preUpdate()
{
	pumpMainThreadContinuations();
}

void ThreadModule::postUpdate()
{
}

void ThreadModule::shutdown()
{
	{
		lock_guard<mutex> schedulerLock(schedulerMutex);
		acceptingJobs = false;
		stopRequested = true;
	}

	schedulerWakeCondition.notify_all();

	for (uint32 workerIndex = 0; workerIndex < static_cast<uint32>(workerThreads.size()); ++workerIndex)
	{
		unique_pointer<Thread>& workerThread = workerThreads[workerIndex];
		if (workerThread == nullptr)
		{
			continue;
		}

		workerThread->requestStop();
	}

	for (uint32 workerIndex = 0; workerIndex < static_cast<uint32>(ioThreads.size()); ++workerIndex)
	{
		unique_pointer<Thread>& ioThread = ioThreads[workerIndex];
		if (ioThread == nullptr)
		{
			continue;
		}

		ioThread->requestStop();
	}

	for (uint32 workerIndex = 0; workerIndex < static_cast<uint32>(backgroundThreads.size()); ++workerIndex)
	{
		unique_pointer<Thread>& backgroundThread = backgroundThreads[workerIndex];
		if (backgroundThread == nullptr)
		{
			continue;
		}

		backgroundThread->requestStop();
	}

	for (uint32 workerIndex = 0; workerIndex < static_cast<uint32>(workerThreads.size()); ++workerIndex)
	{
		unique_pointer<Thread>& workerThread = workerThreads[workerIndex];
		if (workerThread != nullptr)
		{
			workerThread->join();
		}
	}

	for (uint32 workerIndex = 0; workerIndex < static_cast<uint32>(ioThreads.size()); ++workerIndex)
	{
		unique_pointer<Thread>& ioThread = ioThreads[workerIndex];
		if (ioThread != nullptr)
		{
			ioThread->join();
		}
	}

	for (uint32 workerIndex = 0; workerIndex < static_cast<uint32>(backgroundThreads.size()); ++workerIndex)
	{
		unique_pointer<Thread>& backgroundThread = backgroundThreads[workerIndex];
		if (backgroundThread != nullptr)
		{
			backgroundThread->join();
		}
	}

	workerThreads.clear();
	ioThreads.clear();
	backgroundThreads.clear();

	{
		lock_guard<mutex> schedulerLock(schedulerMutex);
		clearSchedulerStateUnlocked();
	}

	setCurrentThreadContext(nullptr);
	mainThreadContext.stopRequested.store(false);
	mainThreadContext.state = ThreadState::created;
	mainThreadContext.nativeThreadId = 0;
	mainThreadContext.logicalThreadIndex = 0;
	mainThreadContext.name.clear();
	frameworkReference = nullptr;
}

JobHandle ThreadModule::submitJob(const JobDesc& jobDesc)
{
	assert(jobDesc.execute != nullptr && "[ThreadModule][Assert] reason=job_execute_missing");
	if (jobDesc.execute == nullptr)
	{
		return {};
	}

	ScheduledJob scheduledJob{
		.debugName = jobDesc.debugName,
		.queueType = jobDesc.queueType,
		.priority = jobDesc.priority,
		.execute = jobDesc.execute,
		.completionCounter = jobDesc.createCompletionHandle ? shared_pointer<JobCounter>(new JobCounter()) : nullptr,
		.dependencyCounter = jobDesc.dependency.counter,
		.batchCounter = jobDesc.batchCounter,
	};
	JobHandle outputHandle{
		.counter = scheduledJob.completionCounter,
	};

	lock_guard<mutex> schedulerLock(schedulerMutex);
	if (!acceptingJobs)
	{
		return {};
	}

	scheduledJob.serial = nextJobSerial++;
	if (scheduledJob.dependencyCounter != nullptr && !scheduledJob.dependencyCounter->isCompleted())
	{
		blockedJobs.push_back(moveValue(scheduledJob));
		return outputHandle;
	}

	enqueueReadyJobUnlocked(moveValue(scheduledJob));
	schedulerWakeCondition.notify_all();
	return outputHandle;
}

void ThreadModule::wait(const JobHandle& jobHandle)
{
	if (!jobHandle.isValid())
	{
		return;
	}

	ThreadContext* currentContext = getCurrentContext();
	const bool runningOnSchedulerWorker = currentContext != nullptr
		&& currentContext->role != ThreadRole::main
		&& currentContext->role != ThreadRole::unknown;
	assert(!runningOnSchedulerWorker && "[ThreadModule][Assert] reason=scheduler_worker_wait_forbidden");
	if (currentContext != nullptr && currentContext->role == ThreadRole::main)
	{
		while (!jobHandle.counter->isCompleted())
		{
			pumpMainThreadContinuations();
			yieldCurrentThreadExecution();
		}

		return;
	}

	jobHandle.counter->wait();
}

bool ThreadModule::isCompleted(const JobHandle& jobHandle) const
{
	return !jobHandle.isValid() || jobHandle.counter->isCompleted();
}

void ThreadModule::pumpMainThreadContinuations()
{
	while (true)
	{
		ScheduledJob continuationJob = {};
		{
			lock_guard<mutex> schedulerLock(schedulerMutex);
			moveReadyBlockedJobsUnlocked();
			if (!popNextReadyJobUnlocked(JobQueueType::mainThreadContinuation, continuationJob))
			{
				return;
			}
		}

		try
		{
			continuationJob.execute();
		}
		catch (...)
		{
			error << "[ThreadModule][JobException] queue=mainThreadContinuation job="
				  << continuationJob.debugName << lineBreak;
		}

		markJobCompleted(continuationJob);
	}
}

ThreadContext* ThreadModule::getCurrentContext()
{
	return getCurrentThreadContext();
}

const ThreadContext* ThreadModule::getCurrentContext() const
{
	return getCurrentThreadContextConst();
}

uint32 ThreadModule::getWorkerThreadCount() const
{
	return static_cast<uint32>(workerThreads.size());
}

uint32 ThreadModule::getIOThreadCount() const
{
	return static_cast<uint32>(ioThreads.size());
}

uint32 ThreadModule::getBackgroundThreadCount() const
{
	return static_cast<uint32>(backgroundThreads.size());
}

bool ThreadModule::createWorkerFleet()
{
	for (uint32 workerIndex = 0; workerIndex < getRecommendedGeneralWorkerThreadCount(); ++workerIndex)
	{
		unique_pointer<Thread> workerThread = nullptr;
		if (!createThread(workerThread, "WorkerThread" + to_string(workerIndex), ThreadRole::worker, JobQueueType::worker))
		{
			return false;
		}

		workerThreads.push_back(moveValue(workerThread));
	}

	for (uint32 workerIndex = 0; workerIndex < getRecommendedDedicatedThreadCount(); ++workerIndex)
	{
		unique_pointer<Thread> ioThread = nullptr;
		if (!createThread(ioThread, "IOThread" + to_string(workerIndex), ThreadRole::io, JobQueueType::io))
		{
			return false;
		}

		ioThreads.push_back(moveValue(ioThread));
	}

	for (uint32 workerIndex = 0; workerIndex < getRecommendedDedicatedThreadCount(); ++workerIndex)
	{
		unique_pointer<Thread> backgroundThread = nullptr;
		if (!createThread(backgroundThread, "BackgroundThread" + to_string(workerIndex), ThreadRole::background, JobQueueType::background))
		{
			return false;
		}

		backgroundThreads.push_back(moveValue(backgroundThread));
	}

	return true;
}

bool ThreadModule::createThread(
	unique_pointer<Thread>& outThread,
	const string& threadName,
	const ThreadRole threadRole,
	const JobQueueType queueType)
{
	outThread.reset(new Thread());
	assert(outThread != nullptr && "[ThreadModule][Assert] reason=thread_allocate_failed");

	const bool createdThread = outThread->create({
		.name = threadName,
		.role = threadRole,
		.backendType = getDefaultThreadBackendType(),
		.autoJoinOnDestroy = true,
	});
	assert(createdThread && "[ThreadModule][Assert] reason=thread_create_failed");
	if (!createdThread)
	{
		outThread.reset();
		return false;
	}

	Thread* threadObject = outThread.get();
	assert(threadObject != nullptr && "[ThreadModule][Assert] reason=thread_missing");
	ThreadContext* threadContext = threadObject->getContext();
	assert(threadContext != nullptr && "[ThreadModule][Assert] reason=thread_context_missing");
	threadContext->logicalThreadIndex = getNextLogicalThreadIndex();
	threadContext->name = threadName;
	threadContext->role = threadRole;
	threadContext->backendType = getDefaultThreadBackendType();
	threadContext->state = ThreadState::created;
	threadContext->stopRequested.store(false);

	const bool startedThread = threadObject->start([this, queueType](ThreadContext& workerThreadContext)
	{
		return workerThreadMain(workerThreadContext, queueType);
	});
	assert(startedThread && "[ThreadModule][Assert] reason=thread_start_failed");
	if (!startedThread)
	{
		outThread.reset();
		return false;
	}

	return true;
}

int32 ThreadModule::workerThreadMain(ThreadContext& threadContext, const JobQueueType queueType)
{
	while (!threadContext.stopRequested.load())
	{
		ScheduledJob scheduledJob = {};
		{
			unique_lock<mutex> schedulerLock(schedulerMutex);
			schedulerWakeCondition.wait(schedulerLock, [this, queueType]()
			{
				return stopRequested || hasReadyJobUnlocked(queueType);
			});

			moveReadyBlockedJobsUnlocked();
			if (stopRequested && !hasReadyJobUnlocked(queueType))
			{
				break;
			}

			if (!popNextReadyJobUnlocked(queueType, scheduledJob))
			{
				continue;
			}
		}

		try
		{
			scheduledJob.execute();
		}
		catch (...)
		{
			error << "[ThreadModule][JobException] queue=" << static_cast<uint32>(queueType)
				  << " job=" << scheduledJob.debugName << lineBreak;
		}

		markJobCompleted(scheduledJob);
	}

	return 0;
}

bool ThreadModule::hasReadyJobUnlocked(const JobQueueType queueType) const
{
	switch (queueType)
	{
	case JobQueueType::worker:
		return !workerQueueState.empty();
	case JobQueueType::io:
		return !ioQueueState.empty();
	case JobQueueType::background:
		return !backgroundQueueState.empty();
	case JobQueueType::mainThreadContinuation:
		return !mainThreadContinuationQueueState.empty();
	default:
		assert(false && "[ThreadModule][Assert] reason=job_queue_type_invalid");
		return false;
	}
}

bool ThreadModule::popNextReadyJobUnlocked(const JobQueueType queueType, ScheduledJob& outJob)
{
	JobQueueState* queueState = nullptr;
	switch (queueType)
	{
	case JobQueueType::worker:
		queueState = &workerQueueState;
		break;
	case JobQueueType::io:
		queueState = &ioQueueState;
		break;
	case JobQueueType::background:
		queueState = &backgroundQueueState;
		break;
	case JobQueueType::mainThreadContinuation:
		queueState = &mainThreadContinuationQueueState;
		break;
	default:
		assert(false && "[ThreadModule][Assert] reason=job_queue_type_invalid");
		return false;
	}

	assert(queueState != nullptr && "[ThreadModule][Assert] reason=job_queue_state_missing");
	for (uint32 priorityIndex = 0; priorityIndex < jobPriorityCount; ++priorityIndex)
	{
		deque<ScheduledJob>& priorityQueue = queueState->priorityQueues[priorityIndex];
		if (priorityQueue.empty())
		{
			continue;
		}

		outJob = moveValue(priorityQueue.front());
		priorityQueue.pop_front();
		return true;
	}

	return false;
}

void ThreadModule::enqueueReadyJobUnlocked(ScheduledJob&& scheduledJob)
{
	JobQueueState* queueState = nullptr;
	switch (scheduledJob.queueType)
	{
	case JobQueueType::worker:
		queueState = &workerQueueState;
		break;
	case JobQueueType::io:
		queueState = &ioQueueState;
		break;
	case JobQueueType::background:
		queueState = &backgroundQueueState;
		break;
	case JobQueueType::mainThreadContinuation:
		queueState = &mainThreadContinuationQueueState;
		break;
	default:
		assert(false && "[ThreadModule][Assert] reason=job_queue_type_invalid");
		return;
	}

	queueState->priorityQueues[getJobPriorityIndex(scheduledJob.priority)].push_back(moveValue(scheduledJob));
}

void ThreadModule::moveReadyBlockedJobsUnlocked()
{
	uint32 blockedJobIndex = 0;
	while (blockedJobIndex < static_cast<uint32>(blockedJobs.size()))
	{
		ScheduledJob& blockedJob = blockedJobs[blockedJobIndex];
		if (blockedJob.dependencyCounter != nullptr && !blockedJob.dependencyCounter->isCompleted())
		{
			++blockedJobIndex;
			continue;
		}

		enqueueReadyJobUnlocked(moveValue(blockedJob));
		blockedJobs.erase(blockedJobs.begin() + blockedJobIndex);
	}
}

void ThreadModule::markJobCompleted(const ScheduledJob& scheduledJob)
{
	if (scheduledJob.completionCounter != nullptr)
	{
		scheduledJob.completionCounter->markCompleted();
	}

	if (scheduledJob.batchCounter != nullptr)
	{
		scheduledJob.batchCounter->markJobCompleted();
	}

	{
		lock_guard<mutex> schedulerLock(schedulerMutex);
		moveReadyBlockedJobsUnlocked();
	}

	schedulerWakeCondition.notify_all();
}

void ThreadModule::clearSchedulerStateUnlocked()
{
	workerQueueState.clear();
	ioQueueState.clear();
	backgroundQueueState.clear();
	mainThreadContinuationQueueState.clear();
	blockedJobs.clear();
}

uint32 ThreadModule::getNextLogicalThreadIndex()
{
	return nextLogicalThreadIndex++;
}

uint32 ThreadModule::getRecommendedGeneralWorkerThreadCount()
{
	const uint32 hardwareThreadCount = static_cast<uint32>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
	const uint32 dedicatedThreadCount = getRecommendedDedicatedThreadCount() * 2;
	if (hardwareThreadCount <= dedicatedThreadCount)
	{
		return 1;
	}

	return hardwareThreadCount - dedicatedThreadCount;
}

uint32 ThreadModule::getRecommendedDedicatedThreadCount()
{
	return 1;
}
