#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

/*
	���䵿��ȭ

	1. ����Ʈ ��ü�� mutex ��ü�� ������ �ִ�.
	2. ����Ʈ ��ü�� ��ŷ�Ѵ�.
*/

class Node 
{
public:
	int key{};
	Node* next{};
	Node() = default;
	Node(int key_value) { key = key_value; }
	~Node() = default;
};

class List 
{
	Node head{ 0x80000000 }, tail{ 0x7FFFFFFF };
	mutex glock{};
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
		pred = &head;
		glock.lock();
		curr = pred->next;
		while (curr->key < key) 
		{
			pred = curr;
			curr = curr->next;
		}
		if (key == curr->key) 
		{
			glock.unlock();
			return false;
		}
		else 
		{
			Node* node{ new Node(key) };
			node->next = curr;
			pred->next = node;
			glock.unlock();
			return true;
		}
	}
	bool remove(int key)
	{
		Node* pred{}, * curr{};
		pred = &head;
		glock.lock();
		curr = pred->next;
		while (curr->key < key) 
		{
			pred = curr;
			curr = curr->next;
		}
		if (key == curr->key) 
		{
			pred->next = curr->next;
			delete curr;
			glock.unlock();
			return true;
		}
		else 
		{
			glock.unlock();
			return false;
		}

	}
	bool contains(int key)
	{
		Node* pred{}, * curr{};
		pred = &head;
		glock.lock();
		curr = pred->next;
		while (curr->key < key) 
		{
			pred = curr;
			curr = curr->next;
		}
		if (key == curr->key) 
		{
			glock.unlock();
			return true;
		}
		else 
		{
			glock.unlock();
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