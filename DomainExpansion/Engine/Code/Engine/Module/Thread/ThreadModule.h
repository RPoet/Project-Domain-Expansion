#pragma once

#include "Engine/Module/Module.h"
#include "Thread/Job.h"
#include "Thread/Thread.h"

class ThreadModule final : public StaticModule<ThreadModule>
{
public:
	ThreadModule()
		: StaticModule("ThreadModule")
	{
	}

	bool initialize(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	JobHandle submitJob(const JobDesc& jobDesc);
	void wait(const JobHandle& jobHandle);
	bool isCompleted(const JobHandle& jobHandle) const;
	void pumpMainThreadContinuations();
	ThreadContext* getCurrentContext();
	const ThreadContext* getCurrentContext() const;
	uint32 getWorkerThreadCount() const;
	uint32 getIOThreadCount() const;
	uint32 getBackgroundThreadCount() const;

private:
	struct ScheduledJob
	{
		uint64 serial = 0;
		string debugName = {};
		JobQueueType queueType = JobQueueType::worker;
		JobPriority priority = JobPriority::normal;
		function<void()> execute = {};
		shared_pointer<JobCounter> completionCounter = nullptr;
		shared_pointer<JobCounter> dependencyCounter = nullptr;
		shared_pointer<JobBatchCounter> batchCounter = nullptr;
	};

	struct JobQueueState
	{
		deque<ScheduledJob> priorityQueues[jobPriorityCount] = {};

		bool empty() const
		{
			for (uint32 priorityIndex = 0; priorityIndex < jobPriorityCount; ++priorityIndex)
			{
				if (!priorityQueues[priorityIndex].empty())
				{
					return false;
				}
			}

			return true;
		}

		void clear()
		{
			for (uint32 priorityIndex = 0; priorityIndex < jobPriorityCount; ++priorityIndex)
			{
				priorityQueues[priorityIndex].clear();
			}
		}
	};

	bool createWorkerFleet();
	bool createThread(
		unique_pointer<Thread>& outThread,
		const string& threadName,
		ThreadRole threadRole,
		JobQueueType queueType);
	int32 workerThreadMain(ThreadContext& threadContext, JobQueueType queueType);
	bool hasReadyJobUnlocked(JobQueueType queueType) const;
	bool popNextReadyJobUnlocked(JobQueueType queueType, ScheduledJob& outJob);
	void enqueueReadyJobUnlocked(ScheduledJob&& scheduledJob);
	void moveReadyBlockedJobsUnlocked();
	void markJobCompleted(const ScheduledJob& scheduledJob);
	void clearSchedulerStateUnlocked();
	uint32 getNextLogicalThreadIndex();
	static uint32 getRecommendedGeneralWorkerThreadCount();
	static uint32 getRecommendedDedicatedThreadCount();

	Framework* frameworkReference = nullptr;
	ThreadContext mainThreadContext = {};
	mutable mutex schedulerMutex = {};
	condition_variable schedulerWakeCondition = {};
	bool acceptingJobs = false;
	bool stopRequested = false;
	uint32 nextLogicalThreadIndex = 1;
	uint64 nextJobSerial = 1;
	vector<unique_pointer<Thread>> workerThreads = {};
	vector<unique_pointer<Thread>> ioThreads = {};
	vector<unique_pointer<Thread>> backgroundThreads = {};
	JobQueueState workerQueueState = {};
	JobQueueState ioQueueState = {};
	JobQueueState backgroundQueueState = {};
	JobQueueState mainThreadContinuationQueueState = {};
	vector<ScheduledJob> blockedJobs = {};
};
