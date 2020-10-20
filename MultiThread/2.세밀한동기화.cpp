#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

/*
	세밀한동기화

	1. 노드 객체가 mutex 객체를 가지고 있다.
	2. 각각의 노드를 개별적으로 락킹한다.
*/

class Node
{
private:
	mutex mtx{};
public:
	int key{};
	Node* next{};
	Node() = default;
	Node(int key_value) { key = key_value; }
	~Node() = default;

	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
};

class List
{
	Node head{ 0x80000000 }, tail{ 0x7FFFFFFF };
public:
	List() { head.next = &tail; }
	~List() {}

	void init()
	{
		Node* ptr{};
		while (head.next != &tail)
		{
			ptr = head.next;
			head.next = head.next->next;
			delete ptr;
		}
	}
	bool add(int key)
	{
		Node* pred{}, * curr{};

		head.lock();
		pred = &head;
		curr = pred->next;
		curr->lock();
		while (curr->key < key)
		{
			pred->unlock();
			pred = curr;
			curr = curr->next;
			curr->lock();
		}
		if (key == curr->key)
		{
			curr->unlock();
			pred->unlock();
			return false;
		}
		else
		{
			Node* node{ new Node(key) };
			node->next = curr;
			pred->next = node;

			curr->unlock();
			pred->unlock();
			return true;
		}
	}
	bool remove(int key)
	{
		Node* pred{}, * curr{};

		head.lock();
		pred = &head;
		curr = pred->next;
		curr->lock();
		while (curr->key < key)
		{
			pred->unlock();
			pred = curr;
			curr = curr->next;
			curr->lock();
		}
		if (key == curr->key)
		{
			pred->next = curr->next;
			curr->unlock();
			pred->unlock();
			delete curr;
			return true;
		}
		else
		{
			curr->unlock();
			pred->unlock();
			return false;
		}
	}
	bool contains(int key)
	{
		Node* pred{}, * curr{};

		head.lock();
		pred = &head;
		curr = pred->next;
		curr->lock();
		while (curr->key < key)
		{
			pred->unlock();
			pred = curr;
			curr = curr->next;
			curr->lock();
		}
		if (key == curr->key)
		{
			curr->unlock();
			pred->unlock();
			return true;
		}
		else
		{
			curr->unlock();
			pred->unlock();
			return false;
		}
	}
	void printElement(int count)
	{
		Node* cur{ head.next };
		for (int i = 0; i < count; ++i)
		{
			if (&tail == cur)
				break;
			cout << cur->key << " ";
			cur = cur->next;
		}
		cout << endl;
	}
};

constexpr int NUM_TEST{ 4000000 };
constexpr int KEY_RANGE{ 1000 };
constexpr int MAX_THREADS{ 8 };

List fList;

void ThreadFunc(int numOfThread)
{
	int key{};

	for (int i = 0; i < NUM_TEST / numOfThread; ++i)
	{
		switch (rand() % 3) {
		case 0:
			key = rand() % KEY_RANGE;
			fList.add(key);
			break;
		case 1:
			key = rand() % KEY_RANGE;
			fList.remove(key);
			break;
		case 2:
			key = rand() % KEY_RANGE;
			fList.contains(key);
			break;
		default: cout << "Error\n";
			exit(-1);
		}
	}
}

int main()
{
	vector<thread> threads{};

	for (int i = 1; i <= MAX_THREADS; i *= 2)
	{
		threads.clear();
		auto start{ high_resolution_clock::now() };

		for (int j = 0; j < i; ++j) threads.emplace_back(ThreadFunc, i);
		for (auto& thread : threads) thread.join();

		fList.printElement(20);

		auto duration{ high_resolution_clock::now() - start };
		cout << i << " Threads Duration = " << duration_cast<milliseconds>(duration).count() << " milliseconds\n";
	}
}