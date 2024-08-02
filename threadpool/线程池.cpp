#include <iostream>
#include <thread>
#include "threadpool.h"
#include <any>

class MyClass : public Task
{
public:

	MyClass(int be, int en)
		: begin(be)
		, end(en)
	{}
	Any run() override
	{
		int sum = 0;
		for (int i = begin; i <= end; ++i) {
			sum += i;
		}
		return sum;
	}

private:
	int begin;
	int end;
};

int main() {
	std::any a = 1;
	ThreadPool pool;
	pool.start(4);
	Result res = pool.submitTask(std::make_shared<MyClass>());
	int sum = res.get().cast_<int>(); // get 返回一个any类型

	getchar();
}