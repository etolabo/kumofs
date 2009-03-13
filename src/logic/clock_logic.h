#ifndef LOGIC_CLOCK_LOGIC__
#define LOGIC_CLOCK_LOGIC__

#include "logic/clock.h"

namespace kumo {


class clock_logic {
public:
	clock_logic() : m_time(time(NULL)) { }
	~clock_logic() { }

	void clock_update_time()
	{
		m_time = time(NULL);
	}

public:
	void clock_update(Clock c)
	{
		m_clock.update(c.get());
	}

	ClockTime clocktime_now() const
	{
		//return ClockTime(m_clock.get(), m_time);  // FIXME
		return ClockTime(m_clock.get(), time(NULL));
	}

	Clock clock_incr()
	{
		return m_clock.get_incr();
	}

	ClockTime clock_incr_clocktime()
	{
		//return ClockTime(m_clock.get_incr(), m_time); // FIXME
		return ClockTime(m_clock.get_incr(), time(NULL)); // FIXME
	}

private:
	Clock m_clock;
	uint32_t m_time;
};


}  // namespace kumo

#endif /* logic/clock_logic.h */

