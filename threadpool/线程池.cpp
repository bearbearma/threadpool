#include <iostream>
#include <thread>
#include "threadpool.h"

class MyClass : public Task
{
public:

	MyClass(int be, int en)
		: begin(be)
		, end(en)
	{}
	void run() override
	{
		int sum = 0;
		for (int i = begin; i <= end; ++i) {
			sum += i;
		}
	}

private:
	int begin;
	int end;
};

int main() {
	ThreadPool pool;
	pool.start(4);
	for (int i = 0; i < 1024; ++i) {
		pool.submitTask(std::make_shared<MyClass>(1, i));
	}
	getchar();
}