#include "threadpool.h"

constexpr size_t TASK_MAX_THRESHHOLD = INT32_MAX;
constexpr size_t Thread_MAX_THRESHHOLD = 30;
constexpr size_t Thread_MAX_IDLE_TIME = 2; // 单位s


/**************线程池方法实现**************/
ThreadPool::ThreadPool()
	:initThreadSize_(6)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning(false)
	, idleThreadSize_(0)
	, threadMaxThreshHold_(Thread_MAX_THRESHHOLD)
	, curThreadSize_(0)
{}

ThreadPool::~ThreadPool()
{
	isPoolRunning = false; 
	std::cout << "Pool End" << std::endl;
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]() -> bool { return threads_.size() == 0;});
}

void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState()) return;
	poolMode_ = mode;
}

void ThreadPool::setTaskQueMaxThreshHold(size_t threshHold)
{
	if (checkRunningState()) return;
	taskQueMaxThreshHold_ = threshHold;
}

Result ThreadPool::submitTask(std::shared_ptr<Task> spTask)
{
	// 获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// 线程的通信，等待任务队列有空余否则进入等待状态,超一秒返回失败
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]() -> bool { return taskQue_.size() < taskQueMaxThreshHold_; }))
	{
		std::cerr << "task queue is full, submit task fail." << std::endl;
		return Result(spTask, false);
	}
	//如果有空余，把任务放入任务队列
	taskQue_.emplace(spTask);
	++taskSize_;
	//新放入任务，任务队列不空，则 notEmpty_ 通知
	notEmpty_.notify_all();
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadMaxThreshHold_)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		size_t threadId = ptr->getId();
		std::cout << "creat new thread!" << std::endl;
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();
		curThreadSize_++;
		idleThreadSize_++;
	}

	return Result(spTask);
}

void ThreadPool::start(size_t initThreadSize)
{
	isPoolRunning = true;

	// 初始化线程个数
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	// 创建线程对象
	for (size_t i = 0; i < initThreadSize_; ++i) 
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		size_t threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}

	// 启动所有线程
	for (size_t i = 0; i < initThreadSize_; ++i) 
	{
		threads_[i]->start(); // 执行一个线程函数
		idleThreadSize_++;
	}
}

void ThreadPool::setThreadMaxThreshHold(int threadSize)
{
	if (checkRunningState()) return;
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadMaxThreshHold_ = threadSize;
	}
}

void ThreadPool::threadFunc(size_t threadId)
{
	auto lastTime = std::chrono::high_resolution_clock().now();

	while (true)
	{
		std::shared_ptr<Task> spTask;
		{   // 获取锁
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "尝试获取任务" << std::endl;

			while (taskQue_.size() == 0) 
			{
				if (!isPoolRunning)
				{
					threads_.erase(threadId);
					std::cout << "threadid: " << std::this_thread::get_id() << " exit!" << std::endl;
					exitCond_.notify_one();
					return;
				}

				if (poolMode_ == PoolMode::MODE_CACHED) 
				{
					if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds> (now - lastTime);
						if (dur.count() >= Thread_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
						{
							threads_.erase(threadId);
							curThreadSize_--;
							idleThreadSize_--;
							return;
						}
					}
				}
				else
				{
					// 等待notEmpty
					notEmpty_.wait(lock);
				}
			}

			idleThreadSize_--;

			std::cout << "获取成功" << std::endl;
			// 非空，从任务队列中取一个任务
			spTask = std::move(taskQue_.front());
			taskQue_.pop();
			--taskSize_;
			// 非空通知
			if (!taskQue_.empty())
			{
				notEmpty_.notify_all();
			}
			// 通知生产者生产
			notFull_.notify_all();
			// 保证任务队列的线程安全后就应该释放锁
		}
		// 当前线程执行改任务
		if (spTask != nullptr)
		{
			spTask->exec();
		}

		std::cout << "执行完成" << std::endl;

		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now();
	}
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning;
}

/**************线程方法实现**************/

size_t Thread::generateId = 0;

Thread::Thread(ThreadFunc func)
	: func_(func)
	, threadId_(generateId++)
{}

Thread::~Thread()
{ 
}

//启动线程
void Thread::start()
{
	// 创建一个线程来执行一个线程函数
	std::thread t(func_, threadId_);
	t.detach(); // 设置成分离线程
}
size_t Thread::getId() const
{
	return threadId_;
}


/**************Result方法实现**************/
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}

Any Result::get()
{
	if (!isValid_)
	{
		return "";
	}
	sem_.acquire();
	return std::move(any_);
}

void Result::setValue(Any any)
{
	this->any_ = std::move(any);
	sem_.release();
}

/**************Task方法实现**************/
void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setValue(run());
	}
}

void Task::setResult(Result* res)
{
	result_ = res;
}

Task::Task()
	: result_(nullptr)
{}