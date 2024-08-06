#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <future>
#include <unordered_map>
#include <condition_variable>

constexpr size_t TASK_MAX_THRESHHOLD = 2;
constexpr size_t Thread_MAX_THRESHHOLD = 2;
constexpr size_t Thread_MAX_IDLE_TIME = 2; // 单位s

// 线程池支持的两种模式
enum class PoolMode	
{
	MODE_FIXED, // 固定线程数量
	MODE_CACHED, // 线程数量可动态增长
};

// 线程类型
class Thread
{
public:
	// 线程函数对象类型
	using ThreadFunc = std::function<void(size_t)>;

	// 线程构造
	Thread(ThreadFunc func)
		: func_(func)
		, threadId_(generateId++)
	{}
	// 线程析构
	~Thread() = default;
	// 启动线程
	void start()
	{
		// 创建一个线程来执行一个线程函数
		std::thread t(func_, threadId_);
		t.detach(); // 设置成分离线程
	}
	// 获取线程id
	size_t getId() const
	{
		return threadId_;
	}

private:
	ThreadFunc func_;
	static size_t generateId;
	size_t threadId_;
};

size_t Thread::generateId = 0;

//线程池类型
class ThreadPool
{
public:
	// 线程池构造
	ThreadPool()
		:initThreadSize_(6)
		, taskSize_(0)
		, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
		, poolMode_(PoolMode::MODE_FIXED)
		, isPoolRunning(false)
		, idleThreadSize_(0)
		, threadMaxThreshHold_(Thread_MAX_THRESHHOLD)
		, curThreadSize_(0)
	{}

	// 线程池析构
	~ThreadPool()
	{
		isPoolRunning = false;
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]() -> bool { return threads_.size() == 0; });
	}

	// 设置线程池工作模式
	void setMode(PoolMode mode)
	{
		if (checkRunningState()) return;
		poolMode_ = mode;
	}

	//设置task任务队列上限阈值
	void setTaskQueMaxThreshHold(size_t threshHold)
	{
		if (checkRunningState()) return;
		taskQueMaxThreshHold_ = threshHold;
	}

	// 设置线程池cached模式下线程阈值
	void setThreadMaxThreshHold(int threadSize)
	{
		if (checkRunningState()) return;
		if (poolMode_ == PoolMode::MODE_CACHED)
		{
			threadMaxThreshHold_ = threadSize;
		}
	}

	// 给线程池提交任务
	template <typename Func, typename...Args> 
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
		);
		std::future<RType> result = task->get_future();

		// 获取锁
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		// 线程的通信，等待任务队列有空余否则进入等待状态,超一秒返回失败
		if (!notFull_.wait_for(lock, std::chrono::seconds(1),
			[&]() -> bool { return taskQue_.size() < taskQueMaxThreshHold_; }))
		{
			std::cerr << "task queue is full, submit task fail." << std::endl;

			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]() -> RType { return RType(); }
			);
			(*task)();
			return task->get_future();
		}
		//如果有空余，把任务放入任务队列
		taskQue_.emplace(
			[task]() -> void {
				// 经典加一层中间层解决问题
				(*task)();
			}
		);
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

		return result;
	}

	// 开启线程池
	void start(size_t initThreadSize = std::thread::hardware_concurrency())
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

	//禁止赋值和拷贝构造
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;

private:
	// 定义线程函数
	void threadFunc(size_t threadid)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();

		while (true)
		{
			Task spTask;
			{   // 获取锁
				std::unique_lock<std::mutex> lock(taskQueMtx_);

				std::cout << "尝试获取任务" << std::endl;

				while (taskQue_.size() == 0)
				{
					if (!isPoolRunning)
					{
						threads_.erase(threadid);
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
								threads_.erase(threadid);
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
				spTask();
			}

			std::cout << "执行完成" << std::endl;

			idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now();
		}
	}

	// 检查运行状态
	bool checkRunningState() const
	{
		return isPoolRunning;
	}

private:
	std::unordered_map<size_t, std::unique_ptr<Thread>> threads_; // 线程列表
	size_t initThreadSize_; //初始线程数量
	size_t threadMaxThreshHold_; // 线程数量上限
	std::atomic_uint64_t curThreadSize_; // 当前线程数量
	std::atomic_uint idleThreadSize_; // 空闲线程数量

	using Task = std::function<void()>;
	std::queue<Task> taskQue_; // 任务队列
	std::atomic_uint taskSize_; // 任务数量
	size_t taskQueMaxThreshHold_; // 任务队列数量上限阈值

	std::mutex taskQueMtx_; // 保证任务队列的线程安全
	std::condition_variable notFull_; // 表示任务队列不满
	std::condition_variable notEmpty_; // 表示任务队列不空
	std::condition_variable exitCond_; // 等待线程资源全部回收

	PoolMode poolMode_; // 设置当前线程池模式
	std::atomic_bool isPoolRunning; // 当前线程池启动状态
};

#endif
