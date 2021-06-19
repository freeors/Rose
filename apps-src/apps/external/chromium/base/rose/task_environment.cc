// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rose/task_environment.h"

#include <algorithm>
#include <memory>

#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/time_domain.h"
#include "base/task/simple_task_executor.h"
#include "base/task/thread_pool/thread_pool_impl.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_local_storage_map.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
// #include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

namespace base {
namespace test {

ObserverList<TaskEnvironment::DestructionObserver>& GetDestructionObservers() {
  static NoDestructor<ObserverList<TaskEnvironment::DestructionObserver>>
      instance;
  return *instance;
}

base::MessagePumpType GetMessagePumpTypeForMainThreadType(
    TaskEnvironment::MainThreadType main_thread_type) {
  switch (main_thread_type) {
    case TaskEnvironment::MainThreadType::DEFAULT:
      return MessagePumpType::DEFAULT;
    case TaskEnvironment::MainThreadType::UI:
      return MessagePumpType::UI;
    case TaskEnvironment::MainThreadType::IO:
      return MessagePumpType::IO;
  }
  NOTREACHED();
  return MessagePumpType::DEFAULT;
}

std::unique_ptr<sequence_manager::SequenceManager>
CreateSequenceManagerForMainThreadType(
    TaskEnvironment::MainThreadType main_thread_type) {
  auto type = GetMessagePumpTypeForMainThreadType(main_thread_type);
  return sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
      MessagePump::Create(type),
      base::sequence_manager::SequenceManager::Settings::Builder()
          .SetMessagePumpType(type)
          .Build());

}  // namespace

class TaskEnvironment::TestTaskTracker
    : public internal::ThreadPoolImpl::TaskTrackerImpl {
 public:
  TestTaskTracker();

  // Allow running tasks. Returns whether tasks were previously allowed to run.
  bool AllowRunTasks();

  // Disallow running tasks. Returns true on success; success requires there to
  // be no tasks currently running. Returns false if >0 tasks are currently
  // running. Prior to returning false, it will attempt to block until at least
  // one task has completed (in an attempt to avoid callers busy-looping
  // DisallowRunTasks() calls with the same set of slowly ongoing tasks). This
  // block attempt will also have a short timeout (in an attempt to prevent the
  // fallout of blocking: if the only task remaining is blocked on the main
  // thread, waiting for it to complete results in a deadlock...).
  bool DisallowRunTasks();

  // Returns true if tasks are currently allowed to run.
  bool TasksAllowedToRun() const;

 private:
  friend class TaskEnvironment;

  // internal::ThreadPoolImpl::TaskTrackerImpl:
  void RunTask(internal::Task task,
               internal::TaskSource* sequence,
               const TaskTraits& traits) override;

  // Synchronizes accesses to members below.
  mutable Lock lock_;

  // True if running tasks is allowed.
  bool can_run_tasks_ GUARDED_BY(lock_) = true;

  // Signaled when |can_run_tasks_| becomes true.
  ConditionVariable can_run_tasks_cv_ GUARDED_BY(lock_);

  // Signaled when a task is completed.
  ConditionVariable task_completed_cv_ GUARDED_BY(lock_);

  // Number of tasks that are currently running.
  int num_tasks_running_ GUARDED_BY(lock_) = 0;

  DISALLOW_COPY_AND_ASSIGN(TestTaskTracker);
};

TaskEnvironment::TaskEnvironment(
    TimeSource time_source,
    MainThreadType main_thread_type,
    ThreadPoolExecutionMode thread_pool_execution_mode,
    ThreadingMode threading_mode,
    ThreadPoolCOMEnvironment thread_pool_com_environment,
    bool subclass_creates_default_taskrunner,
    trait_helpers::NotATraitTag)
    : main_thread_type_(main_thread_type),
      thread_pool_execution_mode_(thread_pool_execution_mode),
      threading_mode_(threading_mode),
      thread_pool_com_environment_(thread_pool_com_environment),
      subclass_creates_default_taskrunner_(subclass_creates_default_taskrunner),
      sequence_manager_(
          CreateSequenceManagerForMainThreadType(main_thread_type)),
      time_overrides_(nullptr),
      mock_clock_(nullptr),
      scoped_lazy_task_runner_list_for_testing_(
          std::make_unique<internal::ScopedLazyTaskRunnerListForTesting>()) {
  CHECK(!base::ThreadTaskRunnerHandle::IsSet());
  // If |subclass_creates_default_taskrunner| is true then initialization is
  // deferred until DeferredInitFromSubclass().
  if (!subclass_creates_default_taskrunner) {
    task_queue_ = sequence_manager_->CreateTaskQueue(
        sequence_manager::TaskQueue::Spec("task_environment_default")
            .SetTimeDomain(nullptr));
    task_runner_ = task_queue_->task_runner();
    sequence_manager_->SetDefaultTaskRunner(task_runner_);
    simple_task_executor_ = std::make_unique<SimpleTaskExecutor>(task_runner_);
    CHECK(base::ThreadTaskRunnerHandle::IsSet())
        << "ThreadTaskRunnerHandle should've been set now.";
    CompleteInitialization();
  }
  sequence_manager_->set_runloop_quit_with_delete_tasks(true);

  if (threading_mode_ != ThreadingMode::MAIN_THREAD_ONLY)
    InitializeThreadPool();

  if (thread_pool_execution_mode_ == ThreadPoolExecutionMode::QUEUED &&
      task_tracker_) {
    CHECK(task_tracker_->DisallowRunTasks());
  }
}

void TaskEnvironment::InitializeThreadPool() {
  CHECK(!ThreadPoolInstance::Get())
      << "Someone has already installed a ThreadPoolInstance. If nothing in "
         "your test does so, then a test that ran earlier may have installed "
         "one and leaked it. base::TestSuite will trap leaked globals, unless "
         "someone has explicitly disabled it with "
         "DisableCheckForLeakedGlobals().";

  ThreadPoolInstance::InitParams init_params(kNumForegroundThreadPoolThreads);
  init_params.suggested_reclaim_time = TimeDelta::Max();
#if defined(OS_WIN)
  if (thread_pool_com_environment_ == ThreadPoolCOMEnvironment::COM_MTA) {
    init_params.common_thread_pool_environment =
        ThreadPoolInstance::InitParams::CommonThreadPoolEnvironment::COM_MTA;
  }
#endif

  auto task_tracker = std::make_unique<TestTaskTracker>();
  task_tracker_ = task_tracker.get();
  auto thread_pool = std::make_unique<internal::ThreadPoolImpl>(
      std::string(), std::move(task_tracker));
  ThreadPoolInstance::Set(std::move(thread_pool));
  ThreadPoolInstance::Get()->Start(init_params);
}

void TaskEnvironment::CompleteInitialization() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  if (main_thread_type() == MainThreadType::IO) {
    file_descriptor_watcher_ =
        std::make_unique<FileDescriptorWatcher>(GetMainThreadTaskRunner());
  }
#endif  // defined(OS_POSIX) || defined(OS_FUCHSIA)
}

TaskEnvironment::TaskEnvironment(TaskEnvironment&& other) = default;

TaskEnvironment::~TaskEnvironment() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // If we've been moved then bail out.
  if (!owns_instance_)
    return;
  for (auto& observer : GetDestructionObservers())
    observer.WillDestroyCurrentTaskEnvironment();
  DestroyThreadPool();
  task_queue_ = nullptr;
  NotifyDestructionObserversAndReleaseSequenceManager();
}

void TaskEnvironment::DestroyThreadPool() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (threading_mode_ == ThreadingMode::MAIN_THREAD_ONLY)
    return;

  // Ideally this would RunLoop().RunUntilIdle() here to catch any errors or
  // infinite post loop in the remaining work but this isn't possible right now
  // because base::~MessageLoop() didn't use to do this and adding it here would
  // make the migration away from MessageLoop that much harder.

  // Without FlushForTesting(), DeleteSoon() and ReleaseSoon() tasks could be
  // skipped, resulting in memory leaks.
  task_tracker_->AllowRunTasks();
  ThreadPoolInstance::Get()->FlushForTesting();
  ThreadPoolInstance::Get()->Shutdown();
  ThreadPoolInstance::Get()->JoinForTesting();
  // Destroying ThreadPoolInstance state can result in waiting on worker
  // threads. Make sure this is allowed to avoid flaking tests that have
  // disallowed waits on their main thread.
  ScopedAllowBaseSyncPrimitivesForTesting allow_waits_to_destroy_task_tracker;
  ThreadPoolInstance::Set(nullptr);
}

sequence_manager::TimeDomain* TaskEnvironment::GetTimeDomain() const {
  return sequence_manager_->GetRealTimeDomain();
}

sequence_manager::SequenceManager* TaskEnvironment::sequence_manager() const {
  DCHECK(subclass_creates_default_taskrunner_);
  return sequence_manager_.get();
}

void TaskEnvironment::DeferredInitFromSubclass(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  task_runner_ = std::move(task_runner);
  sequence_manager_->SetDefaultTaskRunner(task_runner_);
  CompleteInitialization();
}

void TaskEnvironment::NotifyDestructionObserversAndReleaseSequenceManager() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // A derived classes may call this method early.
  if (!sequence_manager_)
    return;

  sequence_manager_.reset();
}

scoped_refptr<base::SingleThreadTaskRunner>
TaskEnvironment::GetMainThreadTaskRunner() {
  DCHECK(task_runner_);
  return task_runner_;
}

bool TaskEnvironment::MainThreadIsIdle() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  sequence_manager::internal::SequenceManagerImpl* sequence_manager_impl =
      static_cast<sequence_manager::internal::SequenceManagerImpl*>(
          sequence_manager_.get());
  // ReclaimMemory sweeps canceled delayed tasks.
  sequence_manager_impl->ReclaimMemory();
  return sequence_manager_impl->IsIdleForTesting();
}

void TaskEnvironment::RunUntilIdle() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  if (threading_mode_ == ThreadingMode::MAIN_THREAD_ONLY) {
    RunLoop(RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
    return;
  }

  // TODO(gab): This can be heavily simplified to essentially:
  //     bool HasMainThreadTasks() {
  //      if (message_loop_)
  //        return !message_loop_->IsIdleForTesting();
  //      return mock_time_task_runner_->NextPendingTaskDelay().is_zero();
  //     }
  //     while (task_tracker_->HasIncompleteTasks() || HasMainThreadTasks()) {
  //       base::RunLoop().RunUntilIdle();
  //       // Avoid busy-looping.
  //       if (task_tracker_->HasIncompleteTasks())
  //         PlatformThread::Sleep(TimeDelta::FromMilliSeconds(1));
  //     }
  // Update: This can likely be done now that MessageLoop::IsIdleForTesting()
  // checks all queues.
  //
  // Other than that it works because once |task_tracker_->HasIncompleteTasks()|
  // is false we know for sure that the only thing that can make it true is a
  // main thread task (TaskEnvironment owns all the threads). As such we can't
  // racily see it as false on the main thread and be wrong as if it the main
  // thread sees the atomic count at zero, it's the only one that can make it go
  // up. And the only thing that can make it go up on the main thread are main
  // thread tasks and therefore we're done if there aren't any left.
  //
  // This simplification further allows simplification of DisallowRunTasks().
  //
  // This can also be simplified even further once TaskTracker becomes directly
  // aware of main thread tasks. https://crbug.com/660078.

  const bool could_run_tasks = task_tracker_->AllowRunTasks();

  for (;;) {
    task_tracker_->AllowRunTasks();

    // First run as many tasks as possible on the main thread in parallel with
    // tasks in ThreadPool. This increases likelihood of TSAN catching
    // threading errors and eliminates possibility of hangs should a
    // ThreadPool task synchronously block on a main thread task
    // (ThreadPoolInstance::FlushForTesting() can't be used here for that
    // reason).
    RunLoop(RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();

    // Then halt ThreadPool. DisallowRunTasks() failing indicates that there
    // were ThreadPool tasks currently running. In that case, try again from
    // top when DisallowRunTasks() yields control back to this thread as they
    // may have posted main thread tasks.
    if (!task_tracker_->DisallowRunTasks())
      continue;

    // Once ThreadPool is halted. Run any remaining main thread tasks (which
    // may have been posted by ThreadPool tasks that completed between the
    // above main thread RunUntilIdle() and ThreadPool DisallowRunTasks()).
    // Note: this assumes that no main thread task synchronously blocks on a
    // ThreadPool tasks (it certainly shouldn't); this call could otherwise
    // hang.
    RunLoop(RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();

    // The above RunUntilIdle() guarantees there are no remaining main thread
    // tasks (the ThreadPool being halted during the last RunUntilIdle() is
    // key as it prevents a task being posted to it racily with it determining
    // it had no work remaining). Therefore, we're done if there is no more work
    // on ThreadPool either (there can be ThreadPool work remaining if
    // DisallowRunTasks() preempted work and/or the last RunUntilIdle() posted
    // more ThreadPool tasks).
    // Note: this last |if| couldn't be turned into a |do {} while();|. A
    // conditional loop makes it such that |continue;| results in checking the
    // condition (not unconditionally loop again) which would be incorrect for
    // the above logic as it'd then be possible for a ThreadPool task to be
    // running during the DisallowRunTasks() test, causing it to fail, but then
    // post to the main thread and complete before the loop's condition is
    // verified which could result in HasIncompleteUndelayedTasksForTesting()
    // returning false and the loop erroneously exiting with a pending task on
    // the main thread.
    if (!task_tracker_->HasIncompleteTaskSourcesForTesting())
      break;
  }

  // The above loop always ends with running tasks being disallowed. Re-enable
  // parallel execution before returning if it was allowed at the beginning of
  // this call.
  if (could_run_tasks)
    task_tracker_->AllowRunTasks();
}

size_t TaskEnvironment::GetPendingMainThreadTaskCount() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // ReclaimMemory sweeps canceled delayed tasks.
  sequence_manager_->ReclaimMemory();
  return sequence_manager_->GetPendingTaskCountForTesting();
}

void TaskEnvironment::DescribePendingMainThreadTasks() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  LOG(INFO) << sequence_manager_->DescribeAllPendingTasks();
}

// static
void TaskEnvironment::AddDestructionObserver(DestructionObserver* observer) {
  GetDestructionObservers().AddObserver(observer);
}

// static
void TaskEnvironment::RemoveDestructionObserver(DestructionObserver* observer) {
  GetDestructionObservers().RemoveObserver(observer);
}

TaskEnvironment::TestTaskTracker::TestTaskTracker()
    : can_run_tasks_cv_(&lock_), task_completed_cv_(&lock_) {
  // Consider threads blocked on these as idle (avoids instantiating
  // ScopedBlockingCalls and confusing some //base internals tests).
  can_run_tasks_cv_.declare_only_used_while_idle();
  task_completed_cv_.declare_only_used_while_idle();
}

bool TaskEnvironment::TestTaskTracker::AllowRunTasks() {
  AutoLock auto_lock(lock_);
  const bool could_run_tasks = can_run_tasks_;
  can_run_tasks_ = true;
  can_run_tasks_cv_.Broadcast();
  return could_run_tasks;
}

bool TaskEnvironment::TestTaskTracker::TasksAllowedToRun() const {
  AutoLock auto_lock(lock_);
  return can_run_tasks_;
}

bool TaskEnvironment::TestTaskTracker::DisallowRunTasks() {
  AutoLock auto_lock(lock_);

  // Can't disallow run task if there are tasks running.
  if (num_tasks_running_ > 0) {
    // Attempt to wait a bit so that the caller doesn't busy-loop with the same
    // set of pending work. A short wait is required to avoid deadlock
    // scenarios. See DisallowRunTasks()'s declaration for more details.
    task_completed_cv_.TimedWait(TimeDelta::FromMilliseconds(1));
    return false;
  }

  can_run_tasks_ = false;
  return true;
}

void TaskEnvironment::TestTaskTracker::RunTask(internal::Task task,
                                               internal::TaskSource* sequence,
                                               const TaskTraits& traits) {
  {
    AutoLock auto_lock(lock_);

    while (!can_run_tasks_)
      can_run_tasks_cv_.Wait();

    ++num_tasks_running_;
  }

  {
    // Using TimeTicksNowIgnoringOverride() because in tests that mock time,
    // Now() can advance very far very fast, and that's not a problem. This is
    // watching for tests that have actually long running tasks which cause our
    // test suites to run slowly.
    base::TimeTicks before = base::subtle::TimeTicksNowIgnoringOverride();
    const Location posted_from = task.posted_from;
    internal::ThreadPoolImpl::TaskTrackerImpl::RunTask(std::move(task),
                                                       sequence, traits);
    base::TimeTicks after = base::subtle::TimeTicksNowIgnoringOverride();
/*
    if ((after - before) > TestTimeouts::action_max_timeout()) {
      ADD_FAILURE() << "TaskEnvironment: RunTask took more than "
                    << TestTimeouts::action_max_timeout().InSeconds()
                    << " seconds. Posted from " << posted_from.ToString();
    }
*/
  }

  {
    AutoLock auto_lock(lock_);

    CHECK_GT(num_tasks_running_, 0);
    CHECK(can_run_tasks_);

    --num_tasks_running_;

    task_completed_cv_.Broadcast();
  }
}

}  // namespace test
}  // namespace base
