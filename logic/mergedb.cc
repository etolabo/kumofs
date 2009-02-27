#include "log/mlogger.h"
#include "log/mlogger_ostream.h"
#include "storage/interface.h"
#include "protocol.h"
#include <iostream>

template <typename T>
struct auto_array {
	auto_array() : m(NULL) { }
	auto_array(T* p) : m(p) { }
	~auto_array() { delete[] m; }
	T& operator[] (size_t i) { return m[i]; }
private:
	T* m;
	auto_array(const auto_array<T>&);
};


using namespace kumo;

struct for_each_update {
	for_each_update(Storage* dstdb, uint64_t* total, uint64_t* merged) :
		m_total(total), m_merged(merged), m_dstdb(dstdb) { }

	void operator() (Storage::iterator& kv)
	{
		++*m_total;

		if(kv.keylen() < Storage::KEY_META_SIZE) { return; }
		if(kv.vallen() < Storage::VALUE_META_SIZE) { return; }

		if( m_dstdb->update(kv.key(), kv.keylen(), kv.val(), kv.vallen()) ) {
			++*m_merged;
		}
	}

private:
	uint64_t *m_total;
	uint64_t *m_merged;
	Storage* m_dstdb;
};


int main(int argc, char* argv[])
{
	if(argc <= 3) {
		std::cerr << "usage: "<<argv[0]<<" <dst.tch> <src.tch>..." << std::endl;
		return 1;
	}

	const char* dst = argv[1];
	unsigned int nsrcs = argc - 2;
	char* const* psrcs = argv + 2;

	mlogger::reset(new mlogger_ostream(mlogger::TRACE, std::cout));

	{
		// init src databases
		auto_array< std::auto_ptr<Storage> > srcdbs(new std::auto_ptr<Storage>[nsrcs]);
		for(unsigned int i=0; i < nsrcs; ++i) {
			srcdbs[i].reset(new Storage(psrcs[i]));
		}
	
		// init dst database
		std::auto_ptr<Storage> dstdb(new Storage(dst));
	
		uint64_t total = 0;
		uint64_t merged = 0;
		for(unsigned int i=0; i < nsrcs; ++i) {
			std::cout << "merging "<<psrcs[i]<< "..." << std::flush;

			srcdbs[i]->for_each( for_each_update(dstdb.get(), &total, &merged) );

			//std::cout << srcdbs[i]->error() << std::endl;  // FIXME
			std::cout << "  merged " << merged << " records of " << total << " records" << std::endl;
		}

		std::cout << "closing "<<dst<<"..." << std::endl;
	}

	std::cout << "done." << std::endl;
	return 0;
}

