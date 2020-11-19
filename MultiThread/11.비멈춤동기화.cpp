#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

/*
	성긴동기화

	1. 스택 객체가 mtx 객체를 가지고 있다.
	2. push와 pop이 같은 lock을 가져야한다.
*/

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

constexpr int NUM_TEST{ 10000000 };
constexpr int MAX_THREADS{ 8 };

Stack stk;

void ThreadFunc(int numOfThread)
{
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