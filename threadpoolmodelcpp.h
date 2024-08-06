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
constexpr size_t Thread_MAX_IDLE_TIME = 2; // ��λs

// �̳߳�֧�ֵ�����ģʽ
enum class PoolMode	
{
	MODE_FIXED, // �̶��߳�����
	MODE_CACHED, // �߳������ɶ�̬����
};

// �߳�����
class Thread
{
public:
	// �̺߳�����������
	using ThreadFunc = std::function<void(size_t)>;

	// �̹߳���
	Thread(ThreadFunc func)
		: func_(func)
		, threadId_(generateId++)
	{}
	// �߳�����
	~Thread() = default;
	// �����߳�
	void start()
	{
		// ����һ���߳���ִ��һ���̺߳���
		std::thread t(func_, threadId_);
		t.detach(); // ���óɷ����߳�
	}
	// ��ȡ�߳�id
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

//�̳߳�����
class ThreadPool
{
public:
	// �̳߳ع���
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

	// �̳߳�����
	~ThreadPool()
	{
		isPoolRunning = false;
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]() -> bool { return threads_.size() == 0; });
	}

	// �����̳߳ع���ģʽ
	void setMode(PoolMode mode)
	{
		if (checkRunningState()) return;
		poolMode_ = mode;
	}

	//����task�������������ֵ
	void setTaskQueMaxThreshHold(size_t threshHold)
	{
		if (checkRunningState()) return;
		taskQueMaxThreshHold_ = threshHold;
	}

	// �����̳߳�cachedģʽ���߳���ֵ
	void setThreadMaxThreshHold(int threadSize)
	{
		if (checkRunningState()) return;
		if (poolMode_ == PoolMode::MODE_CACHED)
		{
			threadMaxThreshHold_ = threadSize;
		}
	}

	// ���̳߳��ύ����
	template <typename Func, typename...Args> 
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
		);
		std::future<RType> result = task->get_future();

		// ��ȡ��
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		// �̵߳�ͨ�ţ��ȴ���������п���������ȴ�״̬,��һ�뷵��ʧ��
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
		//����п��࣬����������������
		taskQue_.emplace(
			[task]() -> void {
				// �����һ���м��������
				(*task)();
			}
		);
		++taskSize_;
		//�·�������������в��գ��� notEmpty_ ֪ͨ
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

	// �����̳߳�
	void start(size_t initThreadSize = std::thread::hardware_concurrency())
	{
		isPoolRunning = true;

		// ��ʼ���̸߳���
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;

		// �����̶߳���
		for (size_t i = 0; i < initThreadSize_; ++i)
		{
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			size_t threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
		}

		// ���������߳�
		for (size_t i = 0; i < initThreadSize_; ++i)
		{
			threads_[i]->start(); // ִ��һ���̺߳���
			idleThreadSize_++;
		}
	}

	//��ֹ��ֵ�Ϳ�������
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;

private:
	// �����̺߳���
	void threadFunc(size_t threadid)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();

		while (true)
		{
			Task spTask;
			{   // ��ȡ��
				std::unique_lock<std::mutex> lock(taskQueMtx_);

				std::cout << "���Ի�ȡ����" << std::endl;

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
						// �ȴ�notEmpty
						notEmpty_.wait(lock);
					}
				}

				idleThreadSize_--;

				std::cout << "��ȡ�ɹ�" << std::endl;
				// �ǿգ������������ȡһ������
				spTask = std::move(taskQue_.front());
				taskQue_.pop();
				--taskSize_;
				// �ǿ�֪ͨ
				if (!taskQue_.empty())
				{
					notEmpty_.notify_all();
				}
				// ֪ͨ����������
				notFull_.notify_all();
				// ��֤������е��̰߳�ȫ���Ӧ���ͷ���
			}
			// ��ǰ�߳�ִ�и�����
			if (spTask != nullptr)
			{
				spTask();
			}

			std::cout << "ִ�����" << std::endl;

			idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now();
		}
	}

	// �������״̬
	bool checkRunningState() const
	{
		return isPoolRunning;
	}

private:
	std::unordered_map<size_t, std::unique_ptr<Thread>> threads_; // �߳��б�
	size_t initThreadSize_; //��ʼ�߳�����
	size_t threadMaxThreshHold_; // �߳���������
	std::atomic_uint64_t curThreadSize_; // ��ǰ�߳�����
	std::atomic_uint idleThreadSize_; // �����߳�����

	using Task = std::function<void()>;
	std::queue<Task> taskQue_; // �������
	std::atomic_uint taskSize_; // ��������
	size_t taskQueMaxThreshHold_; // �����������������ֵ

	std::mutex taskQueMtx_; // ��֤������е��̰߳�ȫ
	std::condition_variable notFull_; // ��ʾ������в���
	std::condition_variable notEmpty_; // ��ʾ������в���
	std::condition_variable exitCond_; // �ȴ��߳���Դȫ������

	PoolMode poolMode_; // ���õ�ǰ�̳߳�ģʽ
	std::atomic_bool isPoolRunning; // ��ǰ�̳߳�����״̬
};

#endif
