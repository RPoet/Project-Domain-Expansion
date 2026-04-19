#pragma once

#include "Thread/ThreadDefinitions.h"

struct JobCounter
{
	mutable mutex completionMutex = {};
	mutable condition_variable completionCondition = {};
	atomic_bool completed = false;

	bool isCompleted() const
	{
		return completed.load();
	}

	void markCompleted()
	{
		{
			lock_guard<mutex> completionLock(completionMutex);
			completed.store(true);
		}

		completionCondition.notify_all();
	}

	void wait() const
	{
		unique_lock<mutex> completionLock(completionMutex);
		completionCondition.wait(completionLock, [this]() { return completed.load(); });
	}
};

struct JobBatchCounter
{
	atomic<uint32> remainingJobCount = 0;
	shared_pointer<JobCounter> completionCounter = shared_pointer<JobCounter>(new JobCounter());

	void initialize(const uint32 jobCount)
	{
		remainingJobCount.store(jobCount);
		if (jobCount == 0 && completionCounter != nullptr)
		{
			completionCounter->markCompleted();
		}
	}

	void markJobCompleted()
	{
		const uint32 previousRemainingJobCount = remainingJobCount.fetch_sub(1);
		assert(previousRemainingJobCount > 0 && "[JobBatchCounter][Assert] reason=remaining_job_count_underflow");
		if (previousRemainingJobCount == 1 && completionCounter != nullptr)
		{
			completionCounter->markCompleted();
		}
	}
};

struct JobHandle
{
	shared_pointer<JobCounter> counter = nullptr;

	bool isValid() const
	{
		return counter != nullptr;
	}
};

struct JobBatch
{
	shared_pointer<JobBatchCounter> counter = nullptr;

	void initialize(const uint32 jobCount)
	{
		counter = shared_pointer<JobBatchCounter>(new JobBatchCounter());
		counter->initialize(jobCount);
	}

	bool isValid() const
	{
		return counter != nullptr;
	}

	JobHandle getHandle() const
	{
		return { .counter = counter != nullptr ? counter->completionCounter : nullptr };
	}
};

struct JobDesc
{
	string debugName = {};
	JobQueueType queueType = JobQueueType::worker;
	JobPriority priority = JobPriority::normal;
	function<void()> execute = {};
	JobHandle dependency = {};
	shared_pointer<JobBatchCounter> batchCounter = nullptr;
	bool createCompletionHandle = true;
};
