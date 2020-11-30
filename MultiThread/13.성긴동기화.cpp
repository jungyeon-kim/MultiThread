#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

/*
	성긴동기화

	1. 리스트 객체가 mtx 객체를 가지고 있다.
	2. 리스트 자체를 락킹한다.
*/

constexpr int MAX_LEVEL{ 8 };

class Node
{
public:
	int key{};
	Node* next[MAX_LEVEL + 1]{};
	int topLevel{ MAX_LEVEL };
public:
	Node() = default;
	Node(int value, int top)
	{
		for (int i = 0; i <= top; ++i) next[i] = nullptr;
		topLevel = top;
		key = value;
	}
	~Node() = default;
};

class SkipList
{
private:
	Node head{}, tail{};
	mutex mtx{};
public:
	SkipList()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		for (auto& i : head.next) i = &tail;
	};
	~SkipList()
	{
		clear();
	}
	
	void clear()
	{
		Node* node{ head.next[0] };
		while (&tail != node)
		{
			Node* target{ node };
			node = node->next[0];
			delete target;
		}
		for (auto& i : head.next) i = &tail;
	}

	void find(int value, Node* pred[], Node* curr[])
	{
		pred[MAX_LEVEL] = &head;
		for (int curLevel = MAX_LEVEL; curLevel >= 0; --curLevel)
		{
			if (curLevel != MAX_LEVEL) pred[curLevel] = pred[curLevel + 1];
			curr[curLevel] = pred[curLevel]->next[curLevel];
			while (curr[curLevel]->key < value)
			{
				pred[curLevel] = curr[curLevel];
				curr[curLevel] = curr[curLevel]->next[curLevel];
			}
		}
	}
	bool add(int value)
	{
		Node* pred[MAX_LEVEL + 1]{};
		Node* curr[MAX_LEVEL + 1]{};

		mtx.lock();
		find(value, pred, curr);

		if (curr[0]->key == value) { mtx.unlock(); return false; }
		else
		{
			int topLevel{};
			while (rand() % 2 == 1) if (++topLevel == MAX_LEVEL) break;

			Node* newNode{ new Node{value, topLevel} };
			for (int i = 0; i <= topLevel; ++i)
			{
				pred[i]->next[i] = newNode;
				newNode->next[i] = curr[i];
			}

			mtx.unlock();
			return true;
		}
	}
	bool remove(int value)
	{
		Node* pred[MAX_LEVEL + 1]{};
		Node* curr[MAX_LEVEL + 1]{};

		mtx.lock();
		find(value, pred, curr);

		if (curr[0]->key == value)
		{
			for (int i = 0; i <= curr[0]->topLevel; ++i) pred[i]->next[i] = curr[0]->next[i];
			delete curr[0];

			mtx.unlock();
			return true;
		}
		else
		{
			mtx.unlock();
			return false;
		}
	}
	bool contain(int value)
	{
		Node* pred[MAX_LEVEL + 1]{};
		Node* curr[MAX_LEVEL + 1]{};

		mtx.lock();
		find(value, pred, curr);

		if (curr[0]->key == value)
		{
			mtx.unlock();
			return true;
		}
		else
		{
			mtx.unlock();
			return false;
		}
	}
	void printElement(int count)
	{
		Node* cur{ head.next[0] };
		for (int i = 0; i < count; ++i)
		{
			if (&tail == cur)
				break;
			cout << cur->key << " ";
			cur = cur->next[0];
		}
		cout << endl;
	}
};

constexpr int NUM_TEST{ 4000000 };
constexpr int KEY_RANGE{ 1000 };
constexpr int MAX_THREADS{ 8 };

SkipList lst;

void ThreadFunc(int numOfThread)
{
	int key{};

	for (int i = 0; i < NUM_TEST / numOfThread; ++i)
	{
		switch (rand() % 3) {
		case 0:
			key = rand() % KEY_RANGE;
			lst.add(key);
			break;
		case 1:
			key = rand() % KEY_RANGE;
			lst.remove(key);
			break;
		case 2:
			key = rand() % KEY_RANGE;
			lst.contain(key);
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

		lst.printElement(20);

		auto duration{ high_resolution_clock::now() - start };
		cout << i << " Threads Duration = " << duration_cast<milliseconds>(duration).count() << " milliseconds\n";
	}
}