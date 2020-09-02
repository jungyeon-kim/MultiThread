#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <chrono> 

using namespace std;
using namespace std::chrono;

/*
	프로세스:	실행중인 프로그램		->	독립 메모리
	스레드:		프로그램 실행의 흐름	->	공유 메모리 (스택 제외)

	Heterogeneous:	작업을 쪼개 용도별로 스레드에 맡기는 스타일 (클라이언트에서 주로 사용)
	Homogeneous:	작업을 쪼개 구분없이 스레드에 맡기는 스타일 (서버에서 주로 사용)

	※ 스레드 각각에 고유한 스택이 존재 (데이터, 코드, 힙은 공유)
	※ 스레드가 많다고 좋은 것이 아님. 코어의 개수에 맞추는게 일반적 (코어보다 많은 스레드 생성시 문맥전환에 대한 오버헤드 발생)
*/

/*
	장점)
	1. 프로세스에 비해 생성, 문맥전환 오버헤드가 적음
	2. 스레드간 데이터를 공유할 수 있음

	단점)
	1. 데이터를 공유하기 때문에, 하나의 스레드에서 발생한 문제가 프로세스까지 영향을 끼칠 수 있음
	2. 스레드 실행순서가 일관적이지 않기때문에 디버깅이 어려움
*/

/*
	주의점)
	DataRace:	같은 메모리를 여러 스레드에서 읽고 써서 의도치 않는 결과가 나오는 것.
				(반드시 하나 이상의 쓰기 동작이 있어야함. 그래야 문제가 발생하기에)
	DeadLock:	무한정으로 대기해 교착상태에 빠지는 것
				ex) 중복 lock, 둘 이상의 스레드가 서로 사용중인 자원을 요구하면서 각자의 자원을 놓지않을 때

	성능을 위해 mutex없이 DataRace를 직접 컨트롤 할 때 메모리 일관성 문제)
	최적화:			컴파일러가 코드를 최적화하면서 문제 발생 -> volatile로 해결
	Write Buffer:	당장에 메모리를 Write할 수 없다면 Buffer에 놔두고 다른 작업을 수행함.
					CPU에서 먼저 실행 가능한 명령부터 수행(비순차적 실행, Out of Order Execution)하면서 문제 발생
					-> volatile로도 해결 불가
	Cash Line:		CPU는 메모리를 64byte로 이루어진 캐시라인 단위로 읽음. (한 캐시라인을 여러 스레드에서 동시에 접근 불가)
					캐시라인의 경계를 침범해 여러 캐시라인을 차지하게 될 경우
					다른 스레드에서 중간 값을 읽어갈 수 있음 -> 포인터나 pragma pack 주의

	※ volatile:		컴파일러의 최적화 옵션이 적용되지 않음
					포인터의 경우 *앞에 오면 포인터가 가리키는 값이 volatile, 뒤에오면 포인터가 volatile
*/

/*
	thread)
	join():		해당 스레드가 종료될 때까지 기다린다. (아직 스레드가 실행중인데 프로그램이 종료되면 안되기 때문)
	detach():	해당 스레드 객체에 연결된 스레드를 떼어낸다. (스레드는 계속 실행되지만 제어할 수 없음, join()불가능)
	joinable():	해당 스레드를 join()으로 기다릴 수 있는 상태인지 bool값을 반환한다. (detach된 상태라면 false 반환)

	mutex)
	lock():		어떤 자원을 다른 스레드에서 사용중이면 대기. 사용중이 아니면 자신의 스레드에서 사용중 표시 (성능저하)
	unLock():	사용중 표시를 지움
	lock_guard<T>:	lock을 걸고 자신이 포함된 스코프를 빠져나갈때 자동으로 unlock

	※ mutex 객체는 전역변수여야함 (같은 자원을 사용하는 스레드마다 mutex도 같은 객체여야하기 때문)

	atomic)
	atomic<T>:							스레드 세이프한 자료형 (mutex보다 빠르지만, 타입제한이 존재)
	atomic_thread_fence():				앞의 명령어들의 메모리 접근이 끝나기 전까지 뒤의 명령어들의
										메모리 접근을 시작하지 못하게 함
	atomic_compare_exchange_strong():	인수1과 인수2가 같으면 인수3을 인수1에 대입하고 true를 반환
										-> CAS (Compare And Set)

	※ Lock 없이 CAS를 이용한 논블로킹 자료구조를 사용해 동기화하는 것이 바람직하다.
*/

// join()과 detach()에 대한 실습코드
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
		if (thread.joinable())	// join()을 호출할 수 있는지 확인
			thread.join();		// detach되면 join()이 호출되지 않기 때문에 스레드 실행도중 프로그램이 종료된다.
}

// 상호배제, 임계영역에 대한 실습코드
atomic<int> atomic_sum{};	// 스레드 공유자원
int sum{};					// 스레드 공유자원
mutex m{};					// mutex 객체

void add()
{
	int localSum{};

	for (int i = 0; i < 1000000; ++i)
	{
		++localSum;		// 부하를 유발하는 lock을 사용하지않고 연산하기 위해 로컬변수를 사용
		++atomic_sum;	// atomic 객체를 사용하면 로컬변수가 필요없음
	}

	m.lock();			// lock을 걸어주지않으면 DataRace에 의해 sum은 예상과 다른 값이 됨
	sum += localSum;	// 로컬변수의 결과를 전역변수(공유자원)에 더해줄때만 lock을 사용
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

// 스레드의 이해를 돕기위한 실습코드
constexpr int MAX_THREADS = 64;

volatile int global{};

void workerThread(int numOfThread)
{
	// 문제 1: 데이터 레이스가 발생해 예상밖의 결과가 나옴
	// 문제 2: 데이터 영역에서 연산하므로 스택 영역에서 연산하는 것 보다 느림
	// solution2()에서 두 문제를 해결.
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