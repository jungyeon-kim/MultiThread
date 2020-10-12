#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <vector>
#include <chrono>

using namespace std;
using namespace std::chrono;

constexpr int MAX_THREADS{ 16 };
volatile int sum{};

class BakeryLock
{
private:
	atomic<bool> flag[MAX_THREADS]{};	// 해당 스레드가 자원을 점유할 준비가 되었는가
	atomic<int> label[MAX_THREADS]{};	// 해당 스레드의 자원점유 우선순위
public:
	void lock(int threadID)
	{
		flag[threadID] = true;
		label[threadID] = *max_element(label, label + MAX_THREADS) + 1;
		for (int i = 0; i < MAX_THREADS; ++i)
		{
			atomic_thread_fence(memory_order_seq_cst);
			if (i != threadID) while (flag[i] && label[i] < label[threadID] && i < threadID) {}
		}
	}
	void unlock(int threadID)
	{
		flag[threadID] = false;
	}
};

BakeryLock BL{};
mutex ML{};

void workerThread(int numOfThread, int threadID)
{
	for (int i = 0; i < 50000000 / numOfThread; ++i)
	{
		BL.lock(threadID);
		//ML.lock();
		sum += 2;
		BL.unlock(threadID);
		//ML.unlock();
	}
}

void solution1()
{
	vector<thread> threads{};

	for (int i = 1; i <= MAX_THREADS; i *= 2)
	{
		sum = 0;
		threads.clear();
		auto start{ high_resolution_clock::now() };

		for (int j = 0; j < i; ++j) threads.emplace_back(workerThread, i, j);
		for (auto& thread : threads) thread.join();

		auto duration{ high_resolution_clock::now() - start };
		cout << i << " Threads" << " Sum = " << sum;
		cout << " Duration = " << duration_cast<milliseconds>(duration).count() << " milliseconds\n";
	}
}

int main()
{
	solution1();
}