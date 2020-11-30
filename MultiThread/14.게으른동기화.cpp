#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

/*
	게으른동기화

	1. 유효성 검사에서 리스트를 순회하지 않는다.
	2. 각 노드마다 리스트에서 제거되었는지 판별하는 마킹변수를 추가 -> isRemoved
	3. add시 모든 노드가 연결이 끝났는지 판단하는 변수가 필요하다. -> isLinkFinished

	※ 마킹은 제거동작보다 먼저 실행되야한다.
*/

constexpr int MAX_LEVEL{ 8 };

class Node
{
private:
	recursive_mutex mtx{};
public:
	int key{};
	int topLevel{ MAX_LEVEL };
	Node* volatile next[MAX_LEVEL + 1]{};
	volatile bool isRemoved{}, isLinkFinished{};
public:
	Node() = default;
	Node(int value, int top)
	{
		topLevel = top;
		key = value;
	}
	~Node() = default;

	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
};

class SkipList
{
private:
	Node head{}, tail{};
public:
	SkipList()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		for (auto& i : head.next) i = &tail;
		head.isLinkFinished = tail.isLinkFinished = true;
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

	int find(int value, Node* pred[], Node* curr[])
	{
		int foundLevel{ -1 };

		pred[MAX_LEVEL] = &head;
		for (int curLevel = MAX_LEVEL; curLevel >= 0; --curLevel)
		{
			if (curLevel != MAX_LEVEL) pred[curLevel] = pred[curLevel + 1];
			curr[curLevel] = pred[curLevel]->next[curLevel];

			if (foundLevel == -1 && curr[curLevel]->key == value) foundLevel = curLevel;

			while (curr[curLevel]->key < value)
			{
				pred[curLevel] = curr[curLevel];
				curr[curLevel] = curr[curLevel]->next[curLevel];
			}
		}

		return foundLevel;
	}
	bool add(int value)
	{
		Node* pred[MAX_LEVEL + 1]{};
		Node* curr[MAX_LEVEL + 1]{};

		while (true)
		{
			int foundLevel{ find(value, pred, curr) };
			if (foundLevel != -1)
			{
				if (curr[0]->isRemoved) continue;
				while (!curr[0]->isLinkFinished);
				return false;
			}

			int curLevel{};
			bool isValid{ true };
			for (curLevel = 0; curLevel <= MAX_LEVEL; ++curLevel)
			{
				pred[curLevel]->lock();
				isValid = !pred[0]->isRemoved && !curr[0]->isRemoved &&
					curr[curLevel] == pred[curLevel]->next[curLevel];
				if (!isValid) break;
			}

			if (!isValid)
			{
				for (int i = 0; i <= curLevel; ++i) pred[i]->unlock();
				continue;
			}
			else
			{
				int topLevel{};
				while (rand() % 2 == 1) if (++topLevel == MAX_LEVEL) break;

				Node* newNode{ new Node{value, topLevel} };
				for (int i = 0; i <= topLevel; ++i) newNode->next[i] = curr[i];
				for (int i = 0; i <= topLevel; ++i) pred[i]->next[i] = newNode;

				newNode->isLinkFinished = true;
				for (int i = 0; i <= MAX_LEVEL; ++i) pred[i]->unlock();
				return true;
			}
		}
	}
	bool remove(int value)
	{
		Node* pred[MAX_LEVEL + 1]{};
		Node* curr[MAX_LEVEL + 1]{};

		int foundLevel{ find(value, pred, curr) };
		if (foundLevel == -1 || curr[0]->isRemoved || !curr[0]->isLinkFinished || curr[0]->topLevel != foundLevel)
			return false;

		curr[0]->lock();
		if (curr[0]->isRemoved) { curr[0]->unlock(); return false; }
		curr[0]->isRemoved = true;

		while (true)
		{
			int curLevel{};
			bool isValid{ true };
			for (curLevel = 0; curLevel <= MAX_LEVEL; ++curLevel)
			{
				pred[curLevel]->lock();
				isValid = !pred[0]->isRemoved && curr[curLevel] == pred[curLevel]->next[curLevel];
				if (!isValid) break;
			}

			if (!isValid)
			{
				for (int i = 0; i <= curLevel; ++i) pred[i]->unlock();
				int foundLevel{ find(value, pred, curr) };
				continue;
			}

			for (int i = curr[0]->topLevel; i >= 0; --i) pred[i]->next[i] = curr[0]->next[i];
			//delete curr[0];

			curr[0]->unlock();
			for (int i = 0; i <= MAX_LEVEL; ++i) pred[i]->unlock();
			return true;
		}
	}
	bool contain(int value)
	{
		Node* pred[MAX_LEVEL + 1]{};
		Node* curr[MAX_LEVEL + 1]{};

		int foundLevel{ find(value, pred, curr) };

		return foundLevel != -1 && curr[foundLevel]->isLinkFinished && !curr[foundLevel]->isRemoved;
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