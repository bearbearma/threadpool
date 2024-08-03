#include "threadpool.h"

constexpr size_t TASK_MAX_THRESHHOLD = 4;
constexpr size_t Thread_MAX_THRESHHOLD = 10;

/**************�̳߳ط���ʵ��**************/
ThreadPool::ThreadPool()
	:initThreadSize_(6)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning(false)
	, idleThreadSize_(0)
	, threadMaxThreshHold_(500)
	, curThreadSize_(0)
{}

ThreadPool::~ThreadPool()
{
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
	// ��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	// �̵߳�ͨ�ţ��ȴ���������п���������ȴ�״̬,��һ�뷵��ʧ��
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]() -> bool { return taskQue_.size() < taskQueMaxThreshHold_; }))
	{
		std::cerr << "task queue is full, submit task fail." << std::endl;
		return Result(spTask, false);
	}
	//����п��࣬����������������
	taskQue_.emplace(spTask);
	++taskSize_;
	//�·�������������в��գ��� notEmpty_ ֪ͨ
	notEmpty_.notify_all();

	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		)

	return Result(spTask);
}

void ThreadPool::start(size_t initThreadSize)
{
	isPoolRunning = true;

	// ��ʼ���̸߳���
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

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
			idleThreadSize_--;

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
			spTask->exec();
		}
		idleThreadSize_++;
	}
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning;
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

/**************Result����ʵ��**************/
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

/**************Task����ʵ��**************/
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