#include <intrin.h>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "Timer.h"

/*
	How does this benchmark work? 

	We basically want to test the speed of loads (e.g. memory speed). Depending on the size of the page, we will get 
	different speed readings. 

	Basically the idea is therefore to only load the data in the program and then throw the results away. However, if 
	we would do that, the compiler will note that the program doesn't do anything - and will eliminate the loop completely. 
	Therefore, we need to have a dummy variable that we're going to use and that the compiler cannot predict. This is
	the role of the 'dummy' variable. However, that would mean that we are benchmarking both a load and a store.

	So why use a xor? Well, unlike most operations, a xor is a very simple operation that has no significant processor overhead.

	The code was also crafted in such a way that the dummy variable itself will be register allocated, and will therefore have
	no significant role in the process. This should always be verified in the assembly output.

	You might also wonder why the 'single threaded' code will process all the data while the AVX2 code does not? Well, 
	since it's all about the loads, this is actually not the case. In both cases we'll load all the data.

	So what about the cache benchmark? Well, cache is loaded from the memory to the L3 cache, all the way to the L1 cache 
	(before	the processor actually uses it). This means that if we hit a single byte in a L1 cache page, we will trigger 
	a cache load. Using this idea, we can perform the benchmark. This seems to be the way that professional software 
	does cache benchmarks... however, since cache lines are only 64 bytes, we have to iterate over the different bytes to 
	be sure -- and even then I think this method is arguably incorrect for a lot of reasons. 
*/

int TestAVX2(long long int size)
{
	long long int bytes = 1024ll * 1024ll * 4096ll;
	long long int count = bytes / (size * 1024ll);

	std::ostringstream oss;
	oss << "Size: " << size << "KB; speed:";

	void* mem = _aligned_malloc((size * 1024), 32);
	int limit = (size * 1024) / 32;

	__m256i dummy = _mm256_set1_epi32(0);
	{
		Util::Timer timer(oss.str().c_str(), bytes);
		for (int i = 0; i < count; ++i)
		{
			// AVX2 load & xor:
			const __m256i* data = (const __m256i*)mem;
			const __m256i* end = (const __m256i*)(((byte*)mem) + size * 1024);
			__m256i dummy2 = _mm256_set1_epi32(0);
			for (; data != end; ++data)
			{
				dummy2 = _mm256_load_si256(data);
			}
			dummy = _mm256_xor_si256(dummy2, dummy);
		}
	}

	_aligned_free(mem);

	return (int)(dummy.m256i_i32[0]);
}

int TestSimple(long long int size)
{
	long long int bytes = 1024ll * 1024ll * 4096ll;
	long long int count = bytes / (size * 1024ll);

	std::ostringstream oss;
	oss << "Size: " << size << "KB; speed:";

	void* mem = _aligned_malloc((size * 1024), 32);
	int limit = (size * 1024) / 32;

	int dummy = 0;
	{
		Util::Timer timer(oss.str().c_str(), bytes);
		for (int i = 0; i < count; ++i)
		{
			// Normal load & and:
			const int* data = (const int*)mem;
			const int* end = (const int*)(((byte*)mem) + size * 1024);

			int dummy2 = 0;
			while (data != end)
			{
				dummy2 = *data++;
			}
			dummy ^= dummy2;
		}
	}

	_aligned_free(mem);

	return (int)(dummy);
}

int TestCacheSpeed(long long int size)
{
	long long int bytes = 1024ll * 1024ll * 4096ll;
	long long int count = bytes / (size * 1024ll);

	std::ostringstream oss;
	oss << "Size: " << size << "KB; speed:";

	void* mem = _aligned_malloc((size * 1024 + 4096), 1024 * 256);
	int limit = (size * 1024) / 32;

	int dummy = 0;
	{
		Util::Timer timer(oss.str().c_str(), bytes);
		for (int i = 0; i < count; ++i)
		{
			// Normal load & and:
			const int* data = (const int*)mem;
			const int* end = (const int*)(((byte*)mem) + size * 1024);

			data += (i % 1024);
			end += (i % 1024);

			int dummy2 = 0;
			while (data != end)
			{
				dummy2 = *data;
				data += 128; // A single cache line is 4K. 
			}
			dummy ^= dummy2;
		}
	}

	_aligned_free(mem);

	return (int)(dummy);
}

struct MTTest
{
	long long int bytes;
	long long int count;
	long long int size;
	void* mem;
	int limit;
	int dummy;

	static void TestSimpleMT2(MTTest *t)
	{
		int dummy = 0;
		long long int count = t->count;
		void* mem = t->mem;
		int size = t->size;
		for (int i = 0; i < count; ++i)
		{
			// Normal load & and:
			const int* data = (const int*)mem;
			const int* end = (const int*)(((byte*)mem) + size * 1024);

			// We're attempting to get the compiler to make dummy2 a register. We need it because 
			// otherwise the complete loop will get eliminated.
			int dummy2 = 0;
			while (data != end)
			{
				dummy2 = (*data++);
			}
			dummy ^= dummy2;
		}
		t->dummy ^= dummy;
	}

	static void TestAVX2MT2(MTTest *t)
	{
		long long int count = t->count;
		void* mem = t->mem;
		int size = t->size;

		__m256i dummy = _mm256_set1_epi32(0);
		for (int i = 0; i < count; ++i)
		{
			// AVX2 load & xor:
			const __m256i* data = (const __m256i*)mem;
			const __m256i* end = (const __m256i*)(((byte*)mem) + size * 1024);

			// We're attempting to get the compiler to make dummy2 a register. We need it because 
			// otherwise the complete loop will get eliminated.
			__m256i dummy2 = _mm256_set1_epi32(0);
			for (; data != end; ++data)
			{
				dummy2 = _mm256_load_si256(data);
			}

			dummy = _mm256_xor_si256(dummy, dummy2);
		}

		t->dummy ^= dummy.m256i_i32[0];
	}

	int TestSimpleMT(long long int size)
	{
		const int numberThreads = 32;

		this->size = size;
		this->bytes = 1024ll * 1024ll * 16ll * 4096ll;
		this->count = (bytes / (size * 1024ll)) / numberThreads;

		std::ostringstream oss;
		oss << "Size: " << size << "KB; speed:";

		mem = _aligned_malloc((size * 1024), 32);
		limit = (size * 1024) / 32;

		dummy = 0;

		std::thread threads[numberThreads];

		{
			Util::Timer timer(oss.str().c_str(), bytes);
			for (int i = 0; i < numberThreads; ++i)
			{
				threads[i] = std::thread(TestSimpleMT2, this);
			}
			for (int i = 0; i < numberThreads; ++i)
			{
				threads[i].join();
			}
		}

		_aligned_free(mem);

		return (int)(dummy);
	}

	int TestAVX2MT(long long int size)
	{
		const int numberThreads = 32;

		this->size = size;
		this->bytes = 1024ll * 1024ll * 16ll * 4096ll;
		this->count = (bytes / (size * 1024ll)) / numberThreads;

		std::ostringstream oss;
		oss << "Size: " << size << "KB; speed:";

		mem = _aligned_malloc((size * 1024), 32);
		limit = (size * 1024) / 32;

		dummy = 0;

		std::thread threads[numberThreads];

		{
			Util::Timer timer(oss.str().c_str(), bytes);
			for (int i = 0; i < numberThreads; ++i)
			{
				threads[i] = std::thread(TestAVX2MT2, this);
			}
			for (int i = 0; i < numberThreads; ++i)
			{
				threads[i].join();
			}
		}

		_aligned_free(mem);

		return (int)(dummy);
	}
};

int main() {

	int total = 0;
	long long int size;

#if defined(_MSC_VER)
	// Bind the application to all CPU's.
	SetThreadAffinityMask(GetCurrentThread(), -1);
#endif

	size = 4;
	std::cout << "Cache benchmark (NOTE: this is arguably incorrect; see code comments):" << std::endl;
	for (int i = 0; i < 20; ++i)
	{
		total += TestCacheSpeed(size);
		size *= 2;
	}

	// Multi-threaded test

	size = 1;
	std::cout << "Normal 32-threaded benchmark:" << std::endl;
	for (int i = 0; i < 20; ++i)
	{
		MTTest mt;
		total += mt.TestSimpleMT(size);
		size *= 2;
	}

	size = 1;
	std::cout << "AVX2 32-threaded benchmark:" << std::endl;
	for (int i = 0; i < 20; ++i)
	{
		MTTest mt;
		total += mt.TestAVX2MT(size);
		size *= 2;
	}

#if defined(_MSC_VER)
	// Bind the application to a single CPU
	SetThreadAffinityMask(GetCurrentThread(), 4);
#endif

	size = 1;
	std::cout << "Normal benchmark:" << std::endl;
	for (int i = 0; i < 20; ++i)
	{
		total += TestSimple(size);
		size *= 2;
	}

	size = 1;
	std::cout << "AVX2 benchmark:" << std::endl;
	for (int i = 0; i < 20; ++i)
	{
		total += TestAVX2(size);
		size *= 2;
	}

//	std::cout << "Stub to ensure we calculate anything: " << total << std::endl;
	std::string s;
	std::getline(std::cin, s);
}