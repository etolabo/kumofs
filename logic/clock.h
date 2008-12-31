#ifndef LOGIC_CLOCK_H__
#define LOGIC_CLOCK_H__

#include <limits>
#include <stdint.h>
#include <time.h>

namespace kumo {


class ClockTime;


class Clock {
public:
	Clock(uint32_t n = 0) : m(n) {}
	~Clock() {}

public:
	ClockTime now_incr();

	uint32_t get_incr()
	{
		//return m++;
		return __sync_fetch_and_add(&m, 1);
	}

	void update(uint32_t o)
	{
		while(true) {
			uint32_t x = m;
			if(!clock_less(x, o)) { return; }
			if(__sync_bool_compare_and_swap(&m, &x, &o)) {
					return;
			}
		}
		//if(clock_less(m, o)) {
		//	m = o;
		//}
	}

	void increment()
	{
		//++m;
		__sync_add_and_fetch(&m, 1);
	}

	bool operator< (const Clock& o) const
	{
		return clock_less(m, o.m);
	}

private:
	static bool clock_less(uint32_t x, uint32_t y)
	{
		if((x < (((uint32_t)1)<<10) && (((uint32_t)1)<<22) < y) ||
		   (y < (((uint32_t)1)<<10) && (((uint32_t)1)<<22) < x)) {
			return x > y;
		} else {
			return x < y;
		}
	}

	friend class ClockTime;

private:
	volatile uint32_t m;
};


class ClockTime {
public:
	ClockTime(uint32_t c, uint32_t t) :
		m( (((uint64_t)t) << 32) | c ) {}

	ClockTime(uint64_t n) : m(n) {}

	~ClockTime() {}

public:
	uint64_t get() const { return m; }

	Clock clock() const {
		return Clock(m&0xffffffff);
	}

	bool operator== (const ClockTime& o) const
	{
		return m == o.m;
	}

	bool operator!= (const ClockTime& o) const
	{
		return !(*this == o);
	}

	bool operator< (const ClockTime& o) const
	{
		return clocktime_less(m, o.m);
	}

	bool operator<= (const ClockTime& o) const
	{
		return (*this == o) || (*this < o);
	}

private:
	static bool clocktime_less(uint64_t x, uint64_t y)
	{
		uint32_t xt = x>>32;
		uint32_t yt = y>>32;
		if(xt == yt) {
			return Clock::clock_less(x&0xffffffff, y&0xffffffff);
		} else {
			return xt < yt;
		}
	}

private:
	uint64_t m;
};


inline ClockTime Clock::now_incr()
{
	return ClockTime(get_incr(), time(NULL));
}


}  // namespace kumo

#endif /* logic/clock.h */

