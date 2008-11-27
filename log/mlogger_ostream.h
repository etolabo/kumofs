#ifndef MLOGGER_OSTREAM_H__
#define MLOGGER_OSTREAM_H__

#include "mlogger.h"

class mlogger_ostream : public mlogger {
public:
	mlogger_ostream(level runtime_level, std::ostream& stream);
	~mlogger_ostream();

	void log_impl(level lv, std::string& str);

private:
	std::ostream& m_stream;
};

#endif /* mlogger_ostream.h */

