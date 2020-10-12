#include <iostream>
#include <mutex>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

/*
	낙천적동기화

	1. 세밀한동기화와 다르게 순회시에는 락킹을 하지않는다.
	2. 순회를 위해 제거된 노드를 delete하지 않는다.
	3. 함수 실행을 위해 pred와 curr을 락킹하였을때 유효성을 검사하고 유효하지않다면 다시 순회한다.
	   -> pred와 curr가 제거되지 않았는가? & pred와 curr사이에 다른 노드가 끼어들지 않았는가?

	※ 유효성 검사에 계속 실패하는 스레드가 있을 수 있다. -> 기아 유발
	※ 제거된 노드를 delete하지 않는다. -> 메모리 릭
*/

class Node
{
	mutex mtx{};
public:
	int key{};
	Node* next{};
	Node() = default;
	Node(int value) { key = value; }
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
		while (true)
		{
			Node* pred{ &head };
			Node* curr{ pred->next };

			while (curr->key < key)
			{
				pred = curr;
				curr = curr->next;
			}

			pred->lock();
			curr->lock();

			if (valid(pred, curr))
			{
				if (key == curr->key)
				{
					pred->unlock();
					curr->unlock();
					return false;
				}
				else
				{
					Node* node{ new Node{key} };
					node->next = curr;
					pred->next = node;

					pred->unlock();
					curr->unlock();
					return true;
				}
			}
			else
			{
				pred->unlock();
				curr->unlock();
			}
		}
	}
	bool remove(int key)
	{
		while (true)
		{
			Node* pred{ &head };
			Node* curr{ pred->next };

			while (curr->key < key)
			{
				pred = curr;
				curr = curr->next;
			}

			pred->lock();
			curr->lock();

			if (valid(pred, curr))
			{
				if (key == curr->key)
				{
					pred->next = curr->next;
					pred->unlock();
					curr->unlock();
					//delete curr;
					return true;
				}
				else
				{
					pred->unlock();
					curr->unlock();
					return false;
				}
			}
			else
			{
				pred->unlock();
				curr->unlock();
			}
		}
	}
	bool contains(int key)
	{
		while (true)
		{
			Node* pred{ &head };
			Node* curr{ pred->next };

			while (curr->key < key)
			{
				pred = curr;
				curr = curr->next;
			}

			pred->lock();
			curr->lock();

			if (valid(pred, curr))
			{
				pred->unlock();
				curr->unlock();
				return key == curr->key;
			}
			else
			{
				pred->unlock();
				curr->unlock();
			}
		}
	}
	bool valid(Node* pred, Node* curr)
	{
		Node* node{ &head };

		while (node->key <= pred->key)
		{
			if (node == pred) return pred->next == curr;
			node = node->next;
		}

		return false;
	}
	void printElement(int count)
	{
		Node* node{ head.next };
		for (int i = 0; i < count; ++i)
		{
			if (&tail == node) break;
			cout << node->key << " ";
			node = node->next;
		}
		cout << "\n";
	}
};

constexpr int NUM_TEST{ 4000000 };
constexpr int KEY_RANGE{ 1000 };
constexpr int MAX_THREADS{ 8 };

List lst;

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
			lst.contains(key);
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