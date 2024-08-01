#include "threadpool.h"

constexpr size_t TASK_MAX_THRESHHOLD = 1024;

/**************线程池方法实现**************/
ThreadPool::ThreadPool()
	:initThreadSize_(6)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
{}

ThreadPool::~ThreadPool()
{
}

void ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
}

void ThreadPool::setTaskQueMaxThreshHold(size_t threshHold)
{
	taskQueMaxThreshHold_ = threshHold;
}

void ThreadPool::submitTask(std::shared_ptr<Task> spTask)
{
	// 获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// 线程的通信，等待任务队列有空余否则进入等待状态
	notFull_.wait(lock, [&]() -> bool {
		return taskQue_.size() < taskQueMaxThreshHold_;
	});
	//如果有空余，把任务放入任务队列
	taskQue_.emplace(spTask);
	++taskSize_;
	//新放入任务，任务队列不空，则 notEmpty_ 通知
	notEmpty_.notify_all();
}

void ThreadPool::start(size_t initThreadSize)
{
	// 初始化线程个数
	initThreadSize_ = initThreadSize;

	// 创建线程对象
	for (size_t i = 0; i < initThreadSize_; ++i) {
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
		threads_.emplace_back(std::move(ptr));
	}

	// 启动所有线程
	for (size_t i = 0; i < initThreadSize_; ++i) {
		threads_[i]->start(); // 执行一个线程函数
	}
}

void ThreadPool::threadFunc()
{
	std::cout << "begin threadFunc tid: " << std::this_thread::get_id() << std::endl;
	std::cout << "end threadFunc tid: " << std::this_thread::get_id() << std::endl;
}

/**************线程方法实现**************/

Thread::Thread(ThreadFunc func)
	: func_(func)
{}

Thread::~Thread()
{ 
}

//启动线程
void Thread::start()
{
	// 创建一个线程来执行一个线程函数
	std::thread t(func_);
	t.detach(); // 设置成分离线程
} 
