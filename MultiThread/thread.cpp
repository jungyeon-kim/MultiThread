#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <chrono> 

using namespace std;
using namespace std::chrono;

/*
	���μ���:	�������� ���α׷�		->	���� �޸�
	������:		���α׷� ������ �帧	->	���� �޸� (���� ����)

	Heterogeneous:	�۾��� �ɰ� �뵵���� �����忡 �ñ�� ��Ÿ�� (Ŭ���̾�Ʈ���� �ַ� ���)
	Homogeneous:	�۾��� �ɰ� ���о��� �����忡 �ñ�� ��Ÿ�� (�������� �ַ� ���)

	�� ������ ������ ������ ������ ���� (������, �ڵ�, ���� ����)
	�� �����尡 ���ٰ� ���� ���� �ƴ�. �ھ��� ������ ���ߴ°� �Ϲ��� (�ھ�� ���� ������ ������ ������ȯ�� ���� ������� �߻�)
*/

/*
	����)
	1. ���μ����� ���� ����, ������ȯ ������尡 ����
	2. �����尣 �����͸� ������ �� ����

	����)
	1. �����͸� �����ϱ� ������, �ϳ��� �����忡�� �߻��� ������ ���μ������� ������ ��ĥ �� ����
	2. ������ ��������� �ϰ������� �ʱ⶧���� ������� �����
*/

/*
	������)
	DataRace:	���� �޸𸮸� ���� �����忡�� �а� �Ἥ �ǵ�ġ �ʴ� ����� ������ ��.
				(�ݵ�� �ϳ� �̻��� ���� ������ �־����. �׷��� ������ �߻��ϱ⿡)
	DeadLock:	���������� ����� �������¿� ������ ��
				ex) �ߺ� lock, �� �̻��� �����尡 ���� ������� �ڿ��� �䱸�ϸ鼭 ������ �ڿ��� �������� ��

	������ ���� mutex���� DataRace�� ���� ��Ʈ�� �� �� �޸� �ϰ��� ����)
	����ȭ:			�����Ϸ��� �ڵ带 ����ȭ�ϸ鼭 ���� �߻� -> volatile�� �ذ�
	Write Buffer:	���忡 �޸𸮸� Write�� �� ���ٸ� Buffer�� ���ΰ� �ٸ� �۾��� ������.
					CPU���� ���� ���� ������ ��ɺ��� ����(������� ����, Out of Order Execution)�ϸ鼭 ���� �߻�
					-> volatile�ε� �ذ� �Ұ�
	Cash Line:		CPU�� �޸𸮸� 64byte�� �̷���� ĳ�ö��� ������ ����. (�� ĳ�ö����� ���� �����忡�� ���ÿ� ���� �Ұ�)
					ĳ�ö����� ��踦 ħ���� ���� ĳ�ö����� �����ϰ� �� ���
					�ٸ� �����忡�� �߰� ���� �о �� ���� -> �����ͳ� pragma pack ����

	�� volatile:		�����Ϸ��� ����ȭ �ɼ��� ������� ����
					�������� ��� *�տ� ���� �����Ͱ� ����Ű�� ���� volatile, �ڿ����� �����Ͱ� volatile
*/

/*
	thread)
	join():		�ش� �����尡 ����� ������ ��ٸ���. (���� �����尡 �������ε� ���α׷��� ����Ǹ� �ȵǱ� ����)
	detach():	�ش� ������ ��ü�� ����� �����带 �����. (������� ��� ��������� ������ �� ����, join()�Ұ���)
	joinable():	�ش� �����带 join()���� ��ٸ� �� �ִ� �������� bool���� ��ȯ�Ѵ�. (detach�� ���¶�� false ��ȯ)

	mutex)
	lock():		� �ڿ��� �ٸ� �����忡�� ������̸� ���. ������� �ƴϸ� �ڽ��� �����忡�� ����� ǥ�� (��������)
	unLock():	����� ǥ�ø� ����
	lock_guard<T>:	lock�� �ɰ� �ڽ��� ���Ե� �������� ���������� �ڵ����� unlock

	�� mutex ��ü�� �������������� (���� �ڿ��� ����ϴ� �����帶�� mutex�� ���� ��ü�����ϱ� ����)

	atomic)
	atomic<T>:							������ �������� �ڷ��� (mutex���� ��������, Ÿ�������� ����)
	atomic_thread_fence():				���� ��ɾ���� �޸� ������ ������ ������ ���� ��ɾ����
										�޸� ������ �������� ���ϰ� ��
	atomic_compare_exchange_strong():	�μ�1�� �μ�2�� ������ �μ�3�� �μ�1�� �����ϰ� true�� ��ȯ
										-> CAS (Compare And Set)

	�� Lock ���� CAS�� �̿��� ����ŷ �ڷᱸ���� ����� ����ȭ�ϴ� ���� �ٶ����ϴ�.
*/

// join()�� detach()�� ���� �ǽ��ڵ�
void printID(int myID)
{
	cout << "My Thread ID: " << myID << endl;
}

void solution1()
{
	vector<thread> v{};

	for (int i = 0; i < 10; ++i) v.emplace_back(printID, i);
	for (auto& thread : v) thread.join();

	v.clear();
	cout << "---------------------------------" << endl;

	for (int i = 0; i < 10; ++i)
	{
		v.emplace_back(printID, i);
		v[i].detach();
	}
	for (auto& thread : v)
		if (thread.joinable())	// join()�� ȣ���� �� �ִ��� Ȯ��
			thread.join();		// detach�Ǹ� join()�� ȣ����� �ʱ� ������ ������ ���൵�� ���α׷��� ����ȴ�.
}

// ��ȣ����, �Ӱ迵���� ���� �ǽ��ڵ�
atomic<int> atomic_sum{};	// ������ �����ڿ�
int sum{};					// ������ �����ڿ�
mutex m{};					// mutex ��ü

void add()
{
	int localSum{};

	for (int i = 0; i < 1000000; ++i)
	{
		++localSum;		// ���ϸ� �����ϴ� lock�� ��������ʰ� �����ϱ� ���� ���ú����� ���
		++atomic_sum;	// atomic ��ü�� ����ϸ� ���ú����� �ʿ����
	}

	m.lock();			// lock�� �ɾ����������� DataRace�� ���� sum�� ����� �ٸ� ���� ��
	sum += localSum;	// ���ú����� ����� ��������(�����ڿ�)�� �����ٶ��� lock�� ���
	m.unlock();
}

void solution2()
{
	vector<thread> v{};

	for (int i = 0; i < 10; ++i) v.emplace_back(add);
	for (auto& thread : v) thread.join();

	cout << "sum: " << sum << endl;
	cout << "atomic_sum: " << atomic_sum << endl;
}

// �������� ���ظ� �������� �ǽ��ڵ�
constexpr int MAX_THREADS = 64;

volatile int global{};

void workerThread(int numOfThread)
{
	// ���� 1: ������ ���̽��� �߻��� ������� ����� ����
	// ���� 2: ������ �������� �����ϹǷ� ���� �������� �����ϴ� �� ���� ����
	// solution2()���� �� ������ �ذ�.
	for (int i = 0; i < 50000000 / numOfThread; ++i) global += 2;
}

void testThread()
{
	vector<thread> threads{};

	for (int i = 1; i <= MAX_THREADS; i *= 2)
	{
		global = 0;
		threads.clear();
		auto start{ high_resolution_clock::now() };

		for (int j = 0; j < i; ++j) threads.emplace_back(thread{ workerThread, i });
		for (auto& tmp : threads) tmp.join();

		auto duration{ high_resolution_clock::now() - start };
		cout << i << " Threads" << " Sum = " << global;
		cout << " Duration = " << duration_cast<milliseconds>(duration).count() << " milliseconds\n";
	}
}

int main()
{
	//solution1();
	solution2();
	//testThread();
}