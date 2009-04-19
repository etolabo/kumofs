#include "gateway/framework.h"
#include "storage/storage.h"

namespace kumo {
namespace gateway {


mod_cache_t::mod_cache_t() : m_db(NULL) { }

mod_cache_t::~mod_cache_t()
{
	if(m_db) {
		tcadbclose(m_db);
		tcadbdel(m_db);
	}
}

void mod_cache_t::init(const char* name)
{
	m_db = tcadbnew();
	if(!m_db) {
		throw std::bad_alloc();
	}

	if(!tcadbopen(m_db, name)) {
		tcadbdel(m_db);
		m_db = NULL;
		throw std::runtime_error("failed to open local cache db");
	}
}

bool mod_cache_t::get_real(const msgtype::DBKey& key, msgtype::DBValue* result_val,
		msgpack::zone* z)
{
	int len;
	char* val = (char*)tcadbget(m_db, key.data(), key.size(), &len);  // FIXME key.raw_data() is invalid
	if(!val) {
		return false;
	}
	if(len < static_cast<int>(Storage::VALUE_META_SIZE)) {
		::free(val);
		return false;
	}

	z->push_finalizer(&::free, (void*)val);
	*result_val = msgtype::DBValue(val, len);

	return true;
}

namespace {
static void* cache_update_proc(const void* vbuf, int vsiz, int *sp, void* op)
{
	const msgtype::DBValue* val =
		reinterpret_cast<const msgtype::DBValue*>(op);

	if(static_cast<size_t>(vsiz) < Storage::VALUE_CLOCKTIME_SIZE ||
			Storage::clocktime_of((const char*)vbuf) < val->clocktime()) {

		void* mem = ::malloc(val->raw_size());
		if(!mem) {
			return NULL;  // FIXME
		}

		*sp = val->raw_size();
		memcpy(mem, val->raw_data(), val->raw_size());
		return mem;

	} else {
		return NULL;
	}
}
}  // noname namespace

void mod_cache_t::update_real(const msgtype::DBKey& key, const msgtype::DBValue& val)
{
	tcadbputproc(m_db,
			key.data(), key.size(),    // FIXME key.raw_data()?
			val.raw_data(), val.raw_size(),
			&cache_update_proc,
			const_cast<void*>(reinterpret_cast<const void*>(&val)));
}


}  // namespace gateway
}  // namespace kumo

