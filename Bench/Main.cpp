#include <intrin.h>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "Timer.h"

int TestAVX2(long long int size)
{
	SetThreadAffinityMask(GetCurrentThread(), 1);
	long long int bytes = 1024ll * 1024ll * 4096ll;
	long long int count = bytes / (size * 1024ll);

	std::ostringstream oss;
	oss << "Size: " << size << "KB; speed:";

	void* mem = _aligned_malloc((size * 1024), 32);
	int limit = (size * 1024) / 32;

	__m256i sum = _mm256_set1_epi32(0);
	{
		Util::Timer timer(oss.str().c_str(), bytes);
		for (int i = 0; i < count; ++i)
		{
			// AVX2 load & xor:
			const __m256i* data = (const __m256i*)mem;
			const __m256i* end = (const __m256i*)(((byte*)mem) + size * 1024);
			for (; data != end; ++data)
			{
				sum = _mm256_xor_si256(sum, _mm256_load_si256(data));
			}
		}
	}

	_aligned_free(mem);

	return (int)(sum.m256i_i32[0]);
}

int TestSimple(long long int size)
{
	long long int bytes = 1024ll * 1024ll * 4096ll;
	long long int count = bytes / (size * 1024ll);

	std::ostringstream oss;
	oss << "Size: " << size << "KB; speed:";

	void* mem = _aligned_malloc((size * 1024), 32);
	int limit = (size * 1024) / 32;

	int sum = 0;
	{
		Util::Timer timer(oss.str().c_str(), bytes);
		for (int i = 0; i < count; ++i)
		{
			// Normal load & and:
			const int* data = (const int*)mem;
			const int* end = (const int*)(((byte*)mem) + size * 1024);
			while (data != end)
			{
				sum += (*data++);
			}
		}
	}

	_aligned_free(mem);

	return (int)(sum);
}

struct MTTest
{
	long long int bytes;
	long long int count;
	long long int size;
	void* mem;
	int limit;
	int sum;

	static void TestSimpleMT2(MTTest *t)
	{
		int sum = 0;
		long long int count = t->count;
		void* mem = t->mem;
		int size = t->size;
		for (int i = 0; i < count; ++i)
		{
			// Normal load & and:
			const int* data = (const int*)mem;
			const int* end = (const int*)(((byte*)mem) + size * 1024);
			while (data != end)
			{
				sum += (*data++);
			}
		}
		t->sum += sum;
	}

	static void TestAVX2MT2(MTTest *t)
	{
		long long int count = t->count;
		void* mem = t->mem;
		int size = t->size;

		__m256i sum = _mm256_set1_epi32(0);
		for (int i = 0; i < count; ++i)
		{
			// AVX2 load & xor:
			const __m256i* data = (const __m256i*)mem;
			const __m256i* end = (const __m256i*)(((byte*)mem) + size * 1024);
			for (; data != end; ++data)
			{
				sum = _mm256_xor_si256(sum, _mm256_load_si256(data));
			}
		}

		t->sum += sum.m256i_i32[0];
	}

	int TestSimpleMT(long long int size)
	{
		SetThreadAffinityMask(GetCurrentThread(), -1);

		const int numberThreads = 32;

		this->size = size;
		this->bytes = 1024ll * 1024ll * 16ll * 4096ll;
		this->count = (bytes / (size * 1024ll)) / numberThreads;

		std::ostringstream oss;
		oss << "Size: " << size << "KB; speed:";

		mem = _aligned_malloc((size * 1024), 32);
		limit = (size * 1024) / 32;

		sum = 0;

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

		return (int)(sum);
	}

	int TestAVX2MT(long long int size)
	{
		SetThreadAffinityMask(GetCurrentThread(), -1);

		const int numberThreads = 32;

		this->size = size;
		this->bytes = 1024ll * 1024ll * 16ll * 4096ll;
		this->count = (bytes / (size * 1024ll)) / numberThreads;

		std::ostringstream oss;
		oss << "Size: " << size << "KB; speed:";

		mem = _aligned_malloc((size * 1024), 32);
		limit = (size * 1024) / 32;

		sum = 0;

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

		return (int)(sum);
	}
};

int main() {

	int total = 0;
	long long int size;

	// Multi-threaded test

	size = 1;
	std::cout << "Normal MT benchmark:" << std::endl;
	for (int i = 0; i < 20; ++i)
	{
		MTTest mt;
		size *= 2;
		total += mt.TestSimpleMT(size);
	}

	size = 1;
	std::cout << "AVX2 MT benchmark:" << std::endl;
	for (int i = 0; i < 20; ++i)
	{
		MTTest mt;
		size *= 2;
		total += mt.TestSimpleMT(size);
	}

	// Bind the application to a single CPU
	SetThreadAffinityMask(GetCurrentThread(), 4);


	size = 1;
	std::cout << "Normal benchmark:" << std::endl;
	for (int i = 0; i < 20; ++i)
	{
		size *= 2;
		total += TestAVX2(size);
	}

	size = 1;
	std::cout << "AVX2 benchmark:" << std::endl;
	for (int i = 0; i < 20; ++i)
	{
		size *= 2;
		total += TestAVX2(size);
	}

	std::cout << "Stub to ensure we calculate anything: " << total << std::endl;
	std::string s;
	std::getline(std::cin, s);
}