#include "log/mlogger.h"
#include "log/mlogger_ostream.h"
#include "storage.h"
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

int main(int argc, char* argv[])
{
	if(argc <= 3) {
		std::cerr << "usage: "<<argv[0]<<" <dst> <src.tch>..." << std::endl;
		return 1;
	}

	using namespace kumo;

	unsigned int nsrc = argc - 3;
	char** psrc = argv + 2;

	std::auto_ptr<Storage> dst(new Storage(argv[1]));

	auto_array< std::auto_ptr<Storage> > srcs(new std::auto_ptr<Storage>[nsrc]);
	for(unsigned int i=0; i < nsrc; ++i) {
		srcs[i].reset(new Storage(psrc[i]));
	}

	for(unsigned int i=0; i < nsrc; ++i) {

		Storage::iterator kv;
		while(srcs[i]->iterator_next(kv)) {
			if(kv.keylen() < DBFormat::VALUE_META_SIZE) { continue; }
			if(kv.vallen() < DBFormat::KEY_META_SIZE) { continue; }

			protocol::type::DBKey key(kv.key(), kv.keylen());
			protocol::type::DBValue val(kv.val(), kv.vallen());

			uint64_t clocktime = 0;
			bool stored = DBFormat::get_clocktime(*dst,
					key.raw_data(), key.raw_size(), &clocktime);

			if(!stored || ClockTime(clocktime) <= ClockTime(val.clocktime())) {
				// stored key is old
				dst->set_async(
						key.raw_data(), key.raw_size(),
						val.raw_data(), val.raw_size());
			}
		}

	}
}

