#include <vector>
#include <mp/zone.h>
#include <memory>

namespace kumo {


class Storage {
public:
	Storage(const char* path);
	~Storage();

public:
	const char* get(const char* key, uint32_t keylen,
			uint32_t* result_vallen,
			mp::zone& z);

	uint32_t get_header(const char* key, uint32_t keylen,
			char* valbuf, uint32_t vallen);

	void set(const char* key, uint32_t keylen,
			const char* val, uint32_t vallen);

	bool del(const char* key, uint32_t keylen);

	class iterator {
	public:
		iterator();
		~iterator();
	public:
		const char* key();
		size_t keylen();
		const char* val();
		size_t vallen();
	public:
		void release_key(mp::zone& z);
		void release_val(mp::zone& z);
	public:
		void reset();
		bool next();
	private:
		void* m_cursor;
		void* m_key;
		void* m_val;
		void clear();
	private:
		iterator(const iterator&);
	};

	void iterator_init(iterator& it);
	bool iterator_next(iterator& it);
	void close();

private:
	static void finalize_clean_data(void* val);
	friend class iterator;
};


}  // namespace kumo

