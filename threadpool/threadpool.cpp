#include "threadpool.h"

constexpr size_t TASK_MAX_THRESHHOLD = 4;

/**************�̳߳ط���ʵ��**************/
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
	// ��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// �̵߳�ͨ�ţ��ȴ���������п���������ȴ�״̬,��һ�뷵��ʧ��
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]() -> bool { return taskQue_.size() < taskQueMaxThreshHold_; }))
	{
		std::cerr << "task queue is full, submit task fail." << std::endl;
		return;
	}
	//����п��࣬����������������
	taskQue_.emplace(spTask);
	++taskSize_;
	//�·�������������в��գ��� notEmpty_ ֪ͨ
	notEmpty_.notify_all();
}

void ThreadPool::start(size_t initThreadSize)
{
	// ��ʼ���̸߳���
	initThreadSize_ = initThreadSize;

	// �����̶߳���
	for (size_t i = 0; i < initThreadSize_; ++i) 
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
		threads_.emplace_back(std::move(ptr));
	}

	// ���������߳�
	for (size_t i = 0; i < initThreadSize_; ++i) 
	{
		threads_[i]->start(); // ִ��һ���̺߳���
	}
}

void ThreadPool::threadFunc()
{
	while (true)
	{
		std::shared_ptr<Task> spTask;

		{   // ��ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid: " << std::this_thread::get_id()
				<< "���Ի�ȡ����..." << std::endl;

			// �ȴ�notEmpty
			notEmpty_.wait(lock, [&]() -> bool {
				return taskQue_.size() > 0;
			});

			std::cout << "tid: " << std::this_thread::get_id()
				<< "��ȡ����ɹ�..." << std::endl;


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
			spTask->run();
		}
	}
}

/**************�̷߳���ʵ��**************/

Thread::Thread(ThreadFunc func)
	: func_(func)
{}

Thread::~Thread()
{ 
}

//�����߳�
void Thread::start()
{
	// ����һ���߳���ִ��һ���̺߳���
	std::thread t(func_);
	t.detach(); // ���óɷ����߳�
} 
