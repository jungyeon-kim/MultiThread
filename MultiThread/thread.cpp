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
	1. 스레드간 데이터를 공유할 수 있음
	2. 공유메모리가 있기에 프로세스에 비해 생성, 문맥전환 오버헤드가 적고 캐시히트율이 높음

	단점)
	1. 데이터를 공유하기 때문에, 하나의 스레드에서 발생한 문제가 프로세스까지 영향을 끼칠 수 있음
	2. 스레드 실행순서가 일관적이지 않기때문에 디버깅이 어려움
*/

/*
	주의점)
	DataRace:	같은 메모리를 여러 스레드에서 읽고 써서 의도치 않은 결과가 나오는 것.
				ex) 레지스터에 변수를 읽어와 연산 후 변수에 저장하려는 도중에 다른 스레드에서 끼어들때
				※ 반드시 하나 이상의 쓰기 동작이 있어야함. 그래야 문제가 발생하니까.
				※ 싱글코어에서도 문맥전환이 일어나면 데이터레이스가 일어날 수 있음
	DeadLock:	무한정으로 대기해 교착상태에 빠지는 것
				ex) 중복 lock, 둘 이상의 스레드가 서로 사용중인 자원을 요구하면서 각자의 자원을 놓지않을 때
	Convoy:		상대적으로 시간이 오래걸리는 스레드가 자원을 점유해 다른 스레드의 해당자원 획득이 지연되는 것
	Lock Convoy:자원을 점유한 스레드가 자원을 반납하지 않았는데 스케줄링에서 제외(문맥전환)되어 
				다른 스레드의 해당자원 획득이 지연되는 것
				코어개수보다 많은 스레드들을 생성할 때 발생
	Starvation:	스레드간의 자원경쟁으로 우선순위가 낮은 스레드가 영원히 자원을 획득하지 못하는 것
	Priority	우선순위가 낮은 스레드가 자원을 점유해 우선순위가 높은 스레드의 실행이 지연되는 것
	Inverse:	필연적인 상황이지만, 우선순위가 낮은 스레드의 점유시간이 길어질수록 성능이 떨어짐

	성능을 위해 lock없이 DataRace를 직접 컨트롤 할 때 메모리 일관성 문제)
	컴파일러 최적화:	컴파일러가 멀티스레드를 고려하지 않고 코드를 최적화하면서 문제 발생 
					ex) 메인스레드: flag = false -> 스레드 1: while(flag) -> 스레드 2: flag = true -> 무한루프
					위 결과는 최적화가 이루어지면서 레지스터에 값을 올려 flag를 갱신하는 과정을 생략하고 비교만 하는게 원인
					-> volatile로 해결
	Write Buffer:	당장에 메모리를 Write할 수 없다면 Buffer에 놔두고 다른 작업을 수행함.
					CPU가 먼저 실행 가능한 명령부터 수행(비순차적 실행, Out of Order Execution)하면서 문제 발생
					메모리에 Write가 완료되었는지 판단해주는 하드웨어가 있어 싱글스레드에서는 문제가 없었지만,
					이는 독립된 코어에서만 동작하기 때문에 멀티코어에서는 문제가 발생할 수 있음
					-> CAS 연산으로 해결 (CPU는 volatile이 뭔지 모름)
	Cash Line:		CPU는 메모리를 64byte로 이루어진 캐시라인 단위로 읽음. (한 캐시라인을 여러 스레드에서 동시에 접근 불가)
					1.	캐시라인의 경계를 침범해 여러 캐시라인을 차지하게 될 경우
						다른 스레드에서 중간 값을 읽어갈 수 있음 -> 포인터, pragma pack 주의
					2.	공유자원이 있는 캐시라인에 접근하기위해 스레드간에 문맥전환이 일어나 성능저하 (Cash Thrashing)
	ABA:			데이터 A가 B로 변경되었는데 다시 A로 변경되는 현상 -> CAS연산과 메모리를 재사용할 때 발생
					ex) 스레드1: A pop 시도 -> 스레드2: A, B pop -> 스레드3: A 메모리를 재사용해 push -> 스레드1: A pop 성공
					위의 결과로 B는 pop되었음에도 불구하고 top이 그 메모리를 가리킴
					-> 스탬프(노드에 카운트 정보를 추가) or shared_ptr로 해결

	※ volatile:		컴파일러의 최적화 옵션이 적용되지 않음 (스레드세이프 용도가 아님)
					포인터의 경우 *앞에 오면 포인터가 가리키는 값이 volatile, 뒤에오면 포인터가 volatile
*/

/*
	thread)
	join():		해당 스레드가 종료될 때까지 기다린다. (아직 스레드가 실행중인데 프로그램이 종료되면 안되기 때문)
				WaitForSingleObject를 대체
	detach():	해당 스레드 객체에 연결된 스레드를 떼어낸다. (스레드는 계속 실행되지만 제어할 수 없음, join()불가능)
	joinable():	해당 스레드를 join()으로 기다릴 수 있는 상태인지 bool값을 반환한다. (detach된 상태라면 false 반환)

	mutex)
	lock():		어떤 자원을 다른 스레드에서 사용중이면 대기. 사용중이 아니면 자신의 스레드에서 사용중 표시
	unLock():	사용중 표시를 지움
	lock_guard<T>:	lock을 걸고 자신이 포함된 스코프를 빠져나갈때 자동으로 unlock

	※ mutex 객체는 전역변수여야함 (같은 자원을 사용하는 스레드마다 mutex도 같은 객체여야하기 때문)
	※ 정확성은 올라가지만, 병렬성이 떨어지고 os를 호출하기 때문에 성능저하 발생
	※ 세마포어와 비슷한 개념이지만, 세마포어는 수용가능한 count만큼 스레드(프로세스)가 임계영역에 들어갈 수 있음

	atomic)
	atomic<T>:							스레드 세이프한 자료형 (mutex보다 빠르지만, 타입제한이 존재)
	atomic_thread_fence():				앞 명령어들의 수행이 끝나기 전까지 뒤 명령어들의 수행을 시작하지 못하게 함
	atomic_compare_exchange_strong():	인수1과 인수2가 같으면 인수3을 인수1에 대입하고 true를 반환
										호출 ~ 완료까지 다른스레드가 끼어들 수 없음
										-> CAS (Compare And Set)
	non-block)
	wait-free:	모든 함수가 정해진 시간에 수행을 끝마침
	lock-free:	하나 이상의 함수가 정해진 시간에 수행을 끝마침

	※	Lock을 이용하면 모든 함수가 정해진 시간에 수행을 끝마칠 수 없다.
		Lock 없이 CAS를 이용한 논블로킹 자료구조를 사용해 동기화하는 것이 바람직하다.
*/

constexpr int MAX_THREADS{ 64 };

// 스레드의 이해를 돕기위한 실습코드
volatile int global{};

void workerThread(int numOfThread)
{
	// 문제1:		데이터 레이스가 발생해 예상밖의 결과가 나옴
	// 문제2:		데이터영역에 접근하면 멀티스레드 환경에서 Cash Thrashing이 일어나 성능이 떨어짐
	// -> solution2()에서 해결
	for (int i = 0; i < 500000000 / numOfThread; ++i) global += 2;
}

void testThread()
{
	vector<thread> threads{};

	for (int i = 1; i <= MAX_THREADS; i *= 2)
	{
		global = 0;
		threads.clear();
		auto start{ high_resolution_clock::now() };

		for (int j = 0; j < i; ++j) threads.emplace_back(workerThread, i);
		for (auto& thread : threads) thread.join();

		auto duration{ high_resolution_clock::now() - start };
		cout << i << " Threads" << " Sum = " << global;
		cout << " Duration = " << duration_cast<milliseconds>(duration).count() << " milliseconds\n";
	}
}

// join()과 detach()에 대한 실습코드
void printID(int myID)
{
	cout << "My Thread ID: " << myID << endl;
}

void solution1()
{
	vector<thread> threads{};

	for (int i = 0; i < 10; ++i) threads.emplace_back(printID, i);
	for (auto& thread : threads) thread.join();

	threads.clear();
	cout << "---------------------------------" << endl;

	for (int i = 0; i < 10; ++i)
	{
		threads.emplace_back(printID, i);
		threads[i].detach();
	}
	for (auto& thread : threads)
		if (thread.joinable())	// join()을 호출할 수 있는지 확인
			thread.join();		// detach되면 join()이 호출되지 않기 때문에 스레드 실행도중 프로그램이 종료된다.
}

// 상호배제, 임계영역에 대한 실습코드
int sum{};					// 스레드 공유자원
atomic<int> atomicSum{};	// 스레드 공유자원
mutex m{};					// mtx 객체

void add(int numOfThread)
{
	volatile int localSum{};	// release 테스트를 위해 volatile 사용

	// 부하를 유발하는 lock을 사용하지않고 연산하기 위해 로컬변수를 사용
	for (int i = 0; i < 500000000 / numOfThread; ++i) localSum += 2;

	m.lock();			// lock을 걸어주지않으면 DataRace에 의해 sum은 예상과 다른 값이 됨
	sum += localSum;	// 로컬변수의 결과를 전역변수(공유자원)에 더해줄때만 lock을 사용
	m.unlock();

	atomicSum += localSum;	// atomic 객체를 사용하면 lock이 필요없음
}

void solution2()
{
	vector<thread> threads{};

	for (int i = 1; i <= MAX_THREADS; i *= 2)
	{
		sum = 0;
		atomicSum = 0;
		threads.clear();
		auto start{ high_resolution_clock::now() };

		for (int j = 0; j < i; ++j) threads.emplace_back(add, i);
		for (auto& thread : threads) thread.join();

		auto duration{ high_resolution_clock::now() - start };
		cout << i << " Threads" << " Sum = " << sum << " AtomicSum = " << atomicSum;
		cout << " Duration = " << duration_cast<milliseconds>(duration).count() << " milliseconds\n";
	}
}

int main()
{
	//testThread();
	//solution1();
	solution2();
}