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
#include <condition_variable>

// ��дAny
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;

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

// ����������
class Task
{
public:
	// �û������Զ��������������ͣ���Task�̳У���дrun������ʵ���Զ���������
	virtual Any run() = 0;
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
	using ThreadFunc = std::function<void()>;

	// �̹߳���
	Thread(ThreadFunc func);
	// �߳�����
	~Thread();
	// �����߳�
	void start();

private:
	ThreadFunc func_;
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
	void submitTask(std::shared_ptr<Task> spTask);
	
	// �����̳߳�
	void start(size_t initThreadSize = 6);

	//��ֹ��ֵ�Ϳ�������
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator = (const ThreadPool&) = delete;

private:
	// �����̺߳���
	void threadFunc();

private:
	std::vector<std::unique_ptr<Thread>> threads_; // �߳��б�
	size_t initThreadSize_; //��ʼ�߳�����
	
	std::queue<std::shared_ptr<Task>> taskQue_; // �������
	std::atomic_uint taskSize_; // ��������
	size_t taskQueMaxThreshHold_; // �����������������ֵ

	std::mutex taskQueMtx_; // ��֤������е��̰߳�ȫ
	std::condition_variable notFull_; // ��ʾ������в���
	std::condition_variable notEmpty_; // ��ʾ������в���

	PoolMode poolMode_; // ���õ�ǰ�̳߳�ģʽ
};

#endif
