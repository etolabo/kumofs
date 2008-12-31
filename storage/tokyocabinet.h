#include <tcutil.h>
#include <tchdb.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <mp/zone.h>
#include <algorithm>

namespace kumo {


class Storage {
public:
	Storage(const char* path)
	{
		m_db = tchdbnew();
		if(!m_db) {
			throw std::runtime_error("can't create tchdb");
		}

		if(!tchdbopen(m_db, path, HDBOWRITER|HDBOCREAT)) {
			tchdbdel(m_db);
			throw std::runtime_error("can't open tchdb");
		}
	}

	~Storage()
	{
		tchdbclose(m_db);
		tchdbdel(m_db);
	}

public:
	const char* get(const char* key, uint32_t keylen,
			uint32_t* result_vallen,
			msgpack::zone& z)
	{
		int vallen = 0;
		char* val = (char*)tchdbget(m_db, key, keylen, &vallen);
		*result_vallen = vallen;
		if(val) {
			try {
				z.push_finalizer(&Storage::finalize_free, val);
			} catch (...) {
				::free(val);
				throw;
			}
			return val;
		} else {
			return NULL;
		}
	}

	// return < 0 if key doesn't exist
	int32_t get_header(const char* key, uint32_t keylen,
			char* valbuf, uint32_t vallen)
	{
		return tchdbget3(m_db, key, keylen, valbuf, vallen);
	}

	void set(const char* key, uint32_t keylen,
			const char* val, uint32_t vallen)
	{
		if(!tchdbput(m_db, key, keylen, val, vallen)) {
			LOG_ERROR("DB set error: ",tchdberrmsg(tchdbecode(m_db)));
			throw std::runtime_error("store failed");
		}
	}

	// return true if deleted
	bool erase(const char* key, uint32_t keylen)
	{
		return tchdbout(m_db, key, keylen);
	}

	struct iterator {
	public:
		iterator() : m_key(NULL), m_val(NULL) { }
		~iterator()
		{
			if(m_key) { tcxstrdel(m_key); }
			if(m_val) { tcxstrdel(m_val); }
		}
	public:
		const char* key()
			{ return TCXSTRPTR(m_key); }
		size_t keylen()
			{ return TCXSTRSIZE(m_key); }
		const char* val()
			{ return TCXSTRPTR(m_val); }
		size_t vallen()
			{ return TCXSTRSIZE(m_val); }
	public:
		void release_key(msgpack::zone& z)
		{
			z.push_finalizer(&Storage::finalize_xstr_del, m_key);
			m_key = NULL;
		}
		void release_val(msgpack::zone& z)
		{
			z.push_finalizer(&Storage::finalize_xstr_del, m_val);
			m_val = NULL;
		}
	private:
		TCXSTR* m_key;
		TCXSTR* m_val;
		void reinit()
		{
			if(!m_key) { m_key = tcxstrnew(); }
			if(!m_val) { m_val = tcxstrnew(); }
		}
		friend class Storage;
	private:
		iterator(const iterator&);
	};

	void iterator_init(iterator& it)
	{
		tchdbiterinit(m_db);
	}

	bool iterator_next(iterator& it)
	{
		it.reinit();
		if( tchdbiternext3(m_db, it.m_key, it.m_val) ) {
			return true;
		} else {
			return false;
		}
	}

	void sync()
	{
		tchdbsync(m_db);
	}

	void close()
	{
		tchdbclose(m_db);
	}

	void copy(const char* dstpath)
	{
		if(!tchdbcopy(m_db, dstpath)) {
			LOG_ERROR("DB copy error: ",tchdberrmsg(tchdbecode(m_db)));
			throw std::runtime_error("copy failed");
		}
	}

private:
	static void finalize_free(void* buf)
	{
		::free(buf);
	}

	static void finalize_xstr_del(void* xstr)
	{
		tcxstrdel(reinterpret_cast<TCXSTR*>(xstr));
	}

public:
	TCHDB* m_db;
};


}  // namespace kumo

