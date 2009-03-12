#ifndef MLOGGER_TTY_H__
#define MLOGGER_TTY_H__

#include "mlogger.h"

class mlogger_tty : public mlogger {
public:
	mlogger_tty(level runtime_level, std::ostream& stream);
	~mlogger_tty();

	void log_impl(level lv, std::string& str);

private:
	std::ostream& m_stream;
};

#endif /* mlogger_tty.h */

