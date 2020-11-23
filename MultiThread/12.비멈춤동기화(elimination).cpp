#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

/*
	성긴동기화(elimination)

	1. push와 pop이 거의 동시에 일어난 경우 연산에서 제외한다. (교환자 구현)
	2. 스레드 충돌빈도에 따라 BackOff를 설정한다.
*/

constexpr int NUM_TEST{ 10000000 };
constexpr int MAX_THREADS{ 8 };
thread_local int NUM_THREADS{};		// 스레드마다 NUM_THREADS 변수가 할당됨

class Exchanger
{
private:
	enum class State { EMPTY, WAITING, BUSY };
private:
	atomic<int> slot{};
private:
	State getState()
	{
		int tmp{ slot };
		return static_cast<State>(tmp >> 30 & 0x3);
	}
	int getValue()
	{
		int tmp{ slot };
		return tmp & 0x7FFFFFFF;
	}
	bool CAS(State oldState, State newState, int oldValue, int newValue)
	{
		int oldSlot{ (static_cast<int>(oldState) << 30) + oldValue };
		int newSlot{ (static_cast<int>(newState) << 30) + newValue };

		return atomic_compare_exchange_strong(&slot, &oldSlot, newSlot);
	}
public:
	int exchange(int val, bool* isTimeOut, bool* isBusy)
	{
		static int numOfLoop{ 100 };

		for(int i = 0; i < numOfLoop; ++i)
		{
			switch (getState())
			{
			case State::EMPTY:
			{
				if (CAS(State::EMPTY, State::WAITING, 0, val))
				{
					int cnt{};
					while (getState() != State::BUSY) 
						if (++cnt > numOfLoop)
						{
							*isTimeOut = true;
							return 0;
						}
					slot = 0;
					return getValue();
				}
				else continue;
			}
			break;
			case State::WAITING:
			{
				int oldValue{ getValue() };
				if (CAS(State::WAITING, State::BUSY, oldValue, val)) return oldValue;
				else continue;
			}
			break;
			case State::BUSY:
				*isBusy = true;
				break;
			}
		}

		*isBusy = true;
		return 0;
	}
};

class BackOff 
{
	int range{ 1 };
	Exchanger exchanger[MAX_THREADS];
public:
	BackOff() = default;
	~BackOff() = default;

	int visit(int value, bool* isTimeOut) 
	{
		int slot{ rand() % range };
		bool isBusy{}; 
		int ret{ exchanger[slot].exchange(value, isTimeOut, &isBusy) };

		if (*isTimeOut && range > 1) --range;
		if (isBusy && range <= NUM_THREADS / 2) ++range;
		return ret;
	}
};

class Node
{
public:
	int key{};
	Node* volatile next{};
	Node() = default;
	Node(int newKey) { key = newKey; }
	~Node() = default;
};

class Stack
{
private:
	BackOff bo{};
	Node* volatile top{};
public:
	Stack() = default;
	~Stack() { init(); }

	void init()
	{
		Node* ptr{};
		while (top)
		{
			ptr = top;
			top = ptr->next;
			delete ptr;
		}
	}

	bool CAS(Node* volatile& addr, const Node* oldNode, const Node* newNode)
	{
		long long oldvalue{ reinterpret_cast<long long>(oldNode) };
		long long newvalue{ reinterpret_cast<long long>(newNode) };
		return atomic_compare_exchange_strong((atomic<long long> volatile*)(&addr), &oldvalue, newvalue);
	}

	void push(int key)
	{
		Node* newNode{ new Node{key} };

		while (true)
		{
			Node* cur{ top };
			newNode->next = cur;
			if (CAS(top, cur, newNode)) return;

			bool isTimeOut{};
			int ret{ bo.visit(key, &isTimeOut) };
			if (!isTimeOut && ret) return;
		}
	}
	int pop()
	{
		while (true)
		{
			Node* cur{ top };
			if (!cur) return -1;

			int val{ cur->key };
			if (CAS(top, cur, cur->next)) { /*delete cur;*/ return val; }

			bool isTimeOut{};
			int ret{ bo.visit(0, &isTimeOut) };
			if (!isTimeOut && ret) return ret;
		}
	}
	void printElement(int count)
	{
		Node* cur{ top };
		for (int i = 0; i < count; ++i)
		{
			if (!cur) break;
			cout << cur->key << " ";
			cur = cur->next;
		}
		cout << endl;
	}
};

Stack stk;

void ThreadFunc(int numOfThread)
{
	NUM_THREADS = numOfThread;

	for (int i = 0; i < NUM_TEST / numOfThread; ++i)
	{
		switch (rand() % 2 || i < 1000 / numOfThread)
		{
		case 0: stk.push(i); break;
		case 1: stk.pop(); break;
		default: cout << "Error\n"; exit(-1);
		}
	}
}

int main()
{
	vector<thread> threads{};

	for (int i = 1; i <= MAX_THREADS; i *= 2)
	{
		threads.clear();
		stk.init();

		auto start{ high_resolution_clock::now() };

		for (int j = 0; j < i; ++j) threads.emplace_back(ThreadFunc, i);
		for (auto& thread : threads) thread.join();

		stk.printElement(20);

		auto duration{ high_resolution_clock::now() - start };
		cout << i << " Threads Duration = " << duration_cast<milliseconds>(duration).count() << " milliseconds\n";
	}
}