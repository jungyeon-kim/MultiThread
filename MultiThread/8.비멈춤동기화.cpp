#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

/*
	비멈춤 동기화

	1. CAS를 이용해 push, pop한다. 실패하면 다음노드에 push, pop하는 로직을 반복
*/

class Node
{
public:
	int key{};
	Node* next{};
	Node() = default;
	Node(int newKey) { key = newKey; }
	~Node() = default;
};

class Queue
{
	Node* volatile head{};
	Node* volatile tail{};
public:
	Queue() { head = tail = new Node{}; }
	~Queue() { init(); delete head; }

	void init()
	{
		Node* ptr{};
		while (head != tail) 
		{
			ptr = head;
			head = head->next;
			delete ptr;
		}
	}

	bool CAS(Node* volatile& addr, const Node* oldNode, const Node* newNode) 
	{
		int oldvalue{ reinterpret_cast<int>(oldNode) };
		int newvalue{ reinterpret_cast<int>(newNode) };
		return atomic_compare_exchange_strong((atomic<int>*)(&addr), &oldvalue, newvalue);
	}
	void push(int key)
	{
		Node* newNode{ new Node{key} };

		while (true)
		{
			Node* cur{ tail };
			Node* next{ cur->next };

			if (cur != tail) continue;
			if (!next)
			{
				if (CAS(cur->next, nullptr, newNode))
				{
					CAS(tail, cur, newNode); return;
				}
			}
			else CAS(tail, cur, next);
		}
	}
	int pop()
	{
		while (true)
		{
			Node* cur{ head };
			Node* next{ cur->next };
			Node* last{ tail };

			if (cur != head) continue;
			if (!next) return -1;
			if (cur == last) { CAS(tail, last, next); continue; }

			int result{ next->key };	// cur은 보초노드이므로 next를 반환
			if (!CAS(head, cur, next)) continue;
			delete cur;
			return result;
		}
	}
	void printElement(int count)
	{
		Node* cur{ head->next };
		while (cur)
		{ 
			cout << cur->key << ", ";
			cur = cur->next;
			if (!(--count)) break;
		} 
		cout << endl;
	}
};

constexpr int NUM_TEST{ 10000000 };
constexpr int MAX_THREADS{ 8 };

Queue que;

void ThreadFunc(int numOfThread)
{
	for (int i = 0; i < NUM_TEST / numOfThread; ++i)
	{
		switch (rand() % 2)
		{
		case 0: que.push(i); break;
		case 1: que.pop(); break;
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
		que.init();

		auto start{ high_resolution_clock::now() };

		for (int j = 0; j < i; ++j) threads.emplace_back(ThreadFunc, i);
		for (auto& thread : threads) thread.join();

		que.printElement(20);

		auto duration{ high_resolution_clock::now() - start };
		cout << i << " Threads Duration = " << duration_cast<milliseconds>(duration).count() << " milliseconds\n";
	}
}