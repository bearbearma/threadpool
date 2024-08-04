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
#include <semaphore>
#include <functional>
#include <unordered_map>
#include <condition_variable>

class Task;
// 手写semaphore
class Semaphore
{
public:
	Semaphore(int limit = 0) 
		: resLimit_(limit)
	{}
	~Semaphore() = default;
	void acquire()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		cond_.wait(lock, [&]() -> bool {
			return resLimit_ > 0;
		});
		resLimit_--;
	}
	void release()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
private:
	size_t resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

// 手写Any
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	template <typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data))
	{}

	template <typename T>
	T cast_()
	{
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->data_;
	}

private:
	// 基类类型
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	// 派生类类型
	template <typename T>
	class Derive : public Base
	{
	public:
		Derive(T data) : data_(data)
		{}
		T data_;
	};

private:
	std::unique_ptr<Base> base_;
};

// Result
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;
	void setValue(Any any);
	Any get();

private:
	Any any_;
	Semaphore sem_;
	std::shared_ptr<Task> task_;
	std::atomic_bool isValid_;
};

// 任务抽象基类
class Task
{
public:
	Task();
	~Task() = default;

	// 用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
	virtual Any run() = 0;
	void exec();
	void setResult(Result*);
private:
	Result* result_;
};

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
	Thread(ThreadFunc func);
	// 线程析构
	~Thread();
	// 启动线程
	void start();
	// 获取线程id
	size_t getId() const;

private:
	ThreadFunc func_;
	static size_t generateId;
	size_t threadId_;
};

//线程池类型
class ThreadPool 
{
public:
	// 线程池构造
	ThreadPool();
	// 线程池析构
	~ThreadPool();
		
	// 设置线程池工作模式
	void setMode(PoolMode mode);
	
	//设置task任务队列上限阈值
	void setTaskQueMaxThreshHold(size_t threshHold);
	
	//给线程池提交任务
	Result submitTask(std::shared_ptr<Task> spTask);
	
	// 开启线程池
	void start(size_t initThreadSize = std::thread::hardware_concurrency());

	// 设置线程池cached模式下线程阈值
	void setThreadMaxThreshHold(int threadSize);

	//禁止赋值和拷贝构造
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;

private:
	// 定义线程函数
	void threadFunc(size_t);

	// 检查运行状态
	bool checkRunningState() const;

private:
	std::unordered_map<size_t, std::unique_ptr<Thread>> threads_; // 线程列表
	size_t initThreadSize_; //初始线程数量
	size_t threadMaxThreshHold_; // 线程数量上限
	std::atomic_uint64_t curThreadSize_; // 当前线程数量
	std::atomic_uint idleThreadSize_; // 空闲线程数量

	std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
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
