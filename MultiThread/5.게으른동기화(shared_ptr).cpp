#include <iostream>
#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

/*
	게으른동기화

	1. remove 대상을 아무도 가리키고있지 않다면 delete해도 된다. -> shared_ptr을 이용해 메모리 릭 해결?
*/

class Node
{
private:
	mutex mtx{};
public:
	int key{};
	bool marked{};
	shared_ptr<Node> next{};
public:
	Node() = default;
	Node(int value) { key = value; }
	~Node() = default;

	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
};

class List
{
private:
	shared_ptr<Node> head{ make_shared<Node>(0x80000000) }, tail{ make_shared<Node>(0x7FFFFFFF) };
public:
	List() { head->next = tail; }
	~List() = default;

	void init() { head->next = tail; }
	bool add(int key)
	{
		while (true)
		{
			shared_ptr<Node> pred{ head };
			shared_ptr<Node> curr{ pred->next };

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
					shared_ptr<Node> node{ make_shared<Node>(key) };
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
			shared_ptr<Node> pred{ head };
			shared_ptr<Node> curr{ pred->next };

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
					curr->marked = true;
					atomic_thread_fence(memory_order_seq_cst);
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
		shared_ptr<Node> node{ head };
		while (node->key < key) node = node->next;
		return node->key == key && !node->marked;
	}
	bool valid(const shared_ptr<Node>& pred, const shared_ptr<Node>& curr)
	{
		return !pred->marked && !curr->marked && pred->next == curr;
	}
	void printElement(int count)
	{
		shared_ptr<Node> node{ head->next };
		for (int i = 0; i < count; ++i)
		{
			if (tail == node) break;
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