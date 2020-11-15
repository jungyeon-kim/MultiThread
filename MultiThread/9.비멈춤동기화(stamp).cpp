#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

/*
	비멈춤동기화(stamp)

	1. 노드에 카운트 정보를 추가한다. 이를 스탬프라 칭한다.
	2. CAS연산 시 스탬프가 일치하지 않는다면, CAS는 실패한다.
*/

#ifdef _WIN64
using integer = long long;
#else
using integer = int;
#endif

class Node
{
public:
	int key{};
	Node* next{};
	Node() = default;
	Node(int newKey) { key = newKey; }
	~Node() = default;
};

// 노드에 스탬프 정보를 추가한 클래스
class Ptr
{
public:
	Node* volatile node{};
	volatile int stamp{};

	Ptr() = default;
	Ptr(Node* newPtr, int newStamp) { node = newPtr; stamp = newStamp; }
};

class Queue
{
	Ptr head{};
	Ptr tail{};
public:
	Queue() { head.node = tail.node = new Node{}; }
	~Queue() = default;

	void init()
	{
		Node* node{};
		while (head.node->next)
		{
			node = head.node->next;
			head.node->next = head.node->next->next;
			delete node;
		}
		tail = head;
	}

	bool CAS(Node* volatile* node, Node* oldNode, Node* newNode)
	{ 
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic<integer>*>(node),
			reinterpret_cast<integer*>(&oldNode),
			reinterpret_cast<integer>(newNode));		// newNode가 포인터이므로 가능
	}
	// CAS에 성공한다면 스탬프를 1 증가
	bool stampCAS(Ptr* node, Node* oldNode, int oldStamp, Node* newNode)
	{ 
		Ptr oldPtr{ oldNode, oldStamp };
		Ptr newPtr{ newNode, oldStamp + 1 };
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic<integer>*>(node),
			reinterpret_cast<integer*>(&oldPtr),
			*(reinterpret_cast<integer*>(&newPtr)));	// newPtr이 포인터가 아니므로 이렇게
	}

	void push(int key)
	{
		Node* newNode{ new Node{key} };

		while (true)
		{
			Ptr cur{ tail };
			Node* next{ cur.node->next };

			if (cur.node != tail.node) continue;
			if (!next)
			{
				if (CAS(&cur.node->next, nullptr, newNode))
				{
					stampCAS(&tail, cur.node, cur.stamp, newNode);
					return;
				}
			}
			else stampCAS(&tail, cur.node, cur.stamp, next);
		}
	}
	int pop()
	{
		while (true)
		{
			Ptr cur{ head };
			Ptr last{ tail };
			Node* next{ cur.node->next };

			if (cur.node != head.node) continue;
			if (!next) return -1;
			if (cur.node == last.node) { stampCAS(&tail, last.node, last.stamp, last.node->next); continue; }

			int result{ next->key };	// cur은 보초노드이므로 next를 반환
			if (!stampCAS(&head, cur.node, cur.stamp, next)) continue;
			delete cur.node;
			return result;
		}
	}
	void printElement(int count)
	{
		Node* cur{ head.node->next };
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