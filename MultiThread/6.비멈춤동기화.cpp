#include <chrono> 
#include <iostream> 
#include <thread> 
#include <mutex> 
#include <vector> 
#include <atomic> 

using namespace std; 
using namespace std::chrono; 

class Node; 

class CPtr
{
private:
	long long value{};
public:
	void set(Node* node, bool removed)
	{
		value = reinterpret_cast<long long>(node);
		if (removed) value = value | 0x01;
		else value = value & 0xFFFFFFFFFFFFFFFE;
	}
	Node* getPtr()
	{
		return reinterpret_cast<Node*>(value & 0xFFFFFFFFFFFFFFFE);
	}
	Node* getPtr(bool* removed)
	{
		if (!(value & 0x01)) *removed = false;
		else *removed = true;
		return reinterpret_cast<Node*>(value & 0xFFFFFFFFFFFFFFFE);
	}
	bool CAS(Node* oldNode, Node* newNode, bool oldRemoved, bool newRemoved)
	{
		long long oldVal{ reinterpret_cast<long long>(oldNode) };
		if (oldRemoved) oldVal = oldVal | 0x01;
		else oldVal = oldVal & 0xFFFFFFFFFFFFFFFE;

		long long newVal{ reinterpret_cast<long long>(newNode) };
		if (oldRemoved) newVal = newVal | 0x01;
		else newVal = newVal & 0xFFFFFFFFFFFFFFFE;

		return atomic_compare_exchange_strong(reinterpret_cast<atomic<long long>*>(&value), &oldVal, newVal);
	}
};

class Node
{
public:
	int key{};
	CPtr next{};
public:
	Node() = default;
	Node(int key_value) { key = key_value; }
	~Node() = default;
};

class List
{
	Node head{ 0x80000000 }, tail{ 0x7FFFFFFF };
public:
	List() { head.next.set(&tail, false); }
	~List() = default;

	void init()
	{
		Node* ptr{};
		while (head.next.getPtr() != &tail)
		{
			ptr = head.next.getPtr();
			head.next.set(ptr->next.getPtr(), false);
			delete ptr;
		}
	}
	void find(Node*& pred, Node*& curr, int key)
	{
	RETRY:
		pred = &head;
		curr = pred->next.getPtr();

		while (true)
		{
			bool isRemoved{};
			Node* succ{ curr->next.getPtr(&isRemoved) };

			while (isRemoved)
			{
				if (!pred->next.CAS(curr, succ, false, false)) goto RETRY;
				curr = succ;
				succ = curr->next.getPtr(&isRemoved);
			} 

			if (curr->key >= key) return;

			pred = curr;
			curr = curr->next.getPtr();
		}
	}
	bool add(int key)
	{
		Node* pred{}, * curr{};

		while (true)
		{
			find(pred, curr, key);

			if (key == curr->key) return false;
			else
			{
				Node* node{ new Node(key) };
				node->next.set(curr, false);
				if (pred->next.CAS(curr, node, false, false)) return true;
			}
		}
	}
	bool remove(int key)
	{
		Node* pred{}, * curr{};

		while (true)
		{
			find(pred, curr, key);

			if (key != curr->key) return false;
			else
			{
				Node* succ{ curr->next.getPtr() };

				if (!pred->next.CAS(curr, curr, false, true)) continue;
				pred->next.CAS(curr, succ, false, false);
				return true;
			}
		}
	}
	bool contatin(int key)
	{
		Node* pred{}, * curr{};
		bool removed{};

		curr = &head;

		while (curr->key < key)
		{
			curr = curr->next.getPtr();
			curr->next.getPtr(&removed);
		}
		return curr->key == key && !removed;
	}
	void printElement(int count)
	{
		Node* node{ head.next.getPtr() };
		while (node != &tail)
		{
			cout << node->key << ", ";
			node = node->next.getPtr();
			--count;
			if (!count) break;
		}
		cout << "\n";
	}
};

const int NUM_TEST{ 4000000 };
const int KEY_RANGE{ 1000 };
constexpr int MAX_THREADS{ 8 };

List lst; 

void ThreadFunc(int numOfThread)
{
	int key{};

	for (int i = 0; i < NUM_TEST / numOfThread; i++)
	{
		switch (rand() % 3)
		{
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
			lst.contatin(key);
			break;
		default:
			cout << "Error\n";
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
		lst.init();

		auto start{ high_resolution_clock::now() };

		for (int j = 0; j < i; ++j) threads.emplace_back(ThreadFunc, i);
		for (auto& thread : threads) thread.join();

		lst.printElement(20);

		auto duration{ high_resolution_clock::now() - start };
		cout << i << " Threads Duration = " << duration_cast<milliseconds>(duration).count() << " milliseconds\n";
	}
}