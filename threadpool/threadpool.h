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
// ��дsemaphore
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

// ��дAny
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
	// ��������
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	// ����������
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

// ����������
class Task
{
public:
	Task();
	~Task() = default;

	// �û������Զ��������������ͣ���Task�̳У���дrun������ʵ���Զ���������
	virtual Any run() = 0;
	void exec();
	void setResult(Result*);
private:
	Result* result_;
};

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
	Thread(ThreadFunc func);
	// �߳�����
	~Thread();
	// �����߳�
	void start();
	// ��ȡ�߳�id
	size_t getId() const;

private:
	ThreadFunc func_;
	static size_t generateId;
	size_t threadId_;
};

//�̳߳�����
class ThreadPool 
{
public:
	// �̳߳ع���
	ThreadPool();
	// �̳߳�����
	~ThreadPool();
		
	// �����̳߳ع���ģʽ
	void setMode(PoolMode mode);
	
	//����task�������������ֵ
	void setTaskQueMaxThreshHold(size_t threshHold);
	
	//���̳߳��ύ����
	Result submitTask(std::shared_ptr<Task> spTask);
	
	// �����̳߳�
	void start(size_t initThreadSize = std::thread::hardware_concurrency());

	// �����̳߳�cachedģʽ���߳���ֵ
	void setThreadMaxThreshHold(int threadSize);

	//��ֹ��ֵ�Ϳ�������
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;

private:
	// �����̺߳���
	void threadFunc(size_t);

	// �������״̬
	bool checkRunningState() const;

private:
	std::unordered_map<size_t, std::unique_ptr<Thread>> threads_; // �߳��б�
	size_t initThreadSize_; //��ʼ�߳�����
	size_t threadMaxThreshHold_; // �߳���������
	std::atomic_uint64_t curThreadSize_; // ��ǰ�߳�����
	std::atomic_uint idleThreadSize_; // �����߳�����

	std::queue<std::shared_ptr<Task>> taskQue_; // �������
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
