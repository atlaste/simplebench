#pragma once

#include <chrono>
#include <string>
#include <iostream>

#if defined(_MSC_VER)

// Sigh...
#include <Windows.h>
#undef min
#undef max

const long long g_Frequency = []() -> long long
{
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	return frequency.QuadPart;
}();

struct HighResClock
{
	typedef long long                               rep;
	typedef std::nano                               period;
	typedef std::chrono::duration<rep, period>      duration;
	typedef std::chrono::time_point<HighResClock>   time_point;
	static const bool is_steady = true;

	static time_point now()
	{
		LARGE_INTEGER count;
		QueryPerformanceCounter(&count);
		return time_point(duration(count.QuadPart * static_cast<rep>(period::den) / g_Frequency));
	}
};

#else

// The way it should have been implemented...
typedef std::chrono::high_resolution_clock HighResClock;

#endif

namespace Util
{
	class Timer
	{
		typedef HighResClock Clock;

		std::string msg;
		long long int count;
		long long int stub;
		Clock::time_point start;
	public:
		Timer(const char* msg, long long int count) : msg(msg), count(count), stub(0)
		{
			start = Clock::now();
		}

		void EndStubPeriod()
		{
			auto end = Clock::now();
			stub += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
			start = Clock::now();
		}

		int64_t Current()
		{
			auto end = Clock::now();
			return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
		}

		~Timer()
		{
			auto end = Clock::now();

			long long millis = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() - stub;

			double g = (double)count;
			double sec = ((double)millis) / 1000.0;

			std::string prefix = "";

			if (count > 1000000000)
			{
				g /= 1000000000.0;
				prefix = "G";
			}
			else if (count > 1000000)
			{
				g /= 1000000.0;
				prefix = "M";
			}
			else if (count > 1000)
			{
				g /= 1000.0;
				prefix = "K";
			}

			double gips = (g / sec);

			std::cout << "Results of " << msg << " test: " << gips << " " << prefix << "B / s (" << millis << "ms)" << std::endl;
		}
	};
}