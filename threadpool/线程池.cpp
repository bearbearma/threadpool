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
		std::this_thread::sleep_for(std::chrono::seconds(2));
		std::cout << "run over" << std::endl;
		return sum;
	}

private:
	int begin;
	int end;
};
 int main() {
	 {
		 ThreadPool pool;
//		 pool.setMode(PoolMode::MODE_CACHED);
		 pool.start(2);
		 Result res1 = pool.submitTask(std::make_shared<MyClass>(1, 100000));
		 pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000)); 
		 pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		 pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		 pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		 pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		 pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		 pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		 pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		 int sum = res1.get().cast_<int>();
//		 std::cout << sum << std::endl;
	 }
	 std::cout << "main over" << std::endl;
#if 0
	{
		ThreadPool pool;
		pool.setMode(PoolMode::MODE_CACHED);
		pool.start(4);
		Result res1 = pool.submitTask(std::make_shared<MyClass>(1, 10000000));
		Result res2 = pool.submitTask(std::make_shared<MyClass>(10000001, 20000000));
		Result res3 = pool.submitTask(std::make_shared<MyClass>(20000001, 3000000));
		pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		pool.submitTask(std::make_shared<MyClass>(2000000001, 300000000));
		int sum1 = res1.get().cast_<int>();
		int sum2 = res2.get().cast_<int>();
		int sum3 = res3.get().cast_<int>();

		std::cout << sum1 + sum2 + sum3 << std::endl;
	}
	getchar();
#endif
}