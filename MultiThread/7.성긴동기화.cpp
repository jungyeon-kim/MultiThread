#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

/*
	성긴동기화

	1. 큐 객체가 mtx 객체를 가지고 있다.
	2. enq lock과 deq lock이 따로 존재한다.
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
	Node* head{}, * tail{};
	mutex pushMtx{}, popMtx{};
public:
	Queue() { head = tail = new Node{}; }
	~Queue() { init(); delete head; }

	void init()
	{
		Node* ptr{};
		while (head != tail)
		{
			ptr = head;
			head = ptr->next;
			delete ptr;
		}
	}
	void push(int key)
	{
		pushMtx.lock();
		tail->next = new Node{ key };
		tail = tail->next;
		pushMtx.unlock();
	}
	int pop()
	{
		popMtx.lock();
		if (head == tail) { popMtx.unlock(); return -1; }
		Node* ptr{ head };
		head = head->next;
		popMtx.unlock();
		delete ptr;
		return head->key;
	}
	void printElement(int count)
	{
		Node* cur{ head->next };
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
constexpr int MAX_THREADS{ 16 };

Queue que;

void ThreadFunc(int numOfThread)
{
	for (int i = 0; i < NUM_TEST / numOfThread; ++i)
	{
		switch (rand() % 2 || i < 2 / numOfThread)
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