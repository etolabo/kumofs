#ifndef SERVER_MMAP_STREAM_H__
#define SERVER_MMAP_STREAM_H__

#include <zlib.h>
#include <mp/exception.h>

namespace kumo {
namespace server {


class zmmap_stream {
public:
	zmmap_stream(int fd);
	~zmmap_stream();
	size_t size() const;

	void write(const void* buf, size_t len);
	void flush();

private:
	z_stream m_z;

	char* m_map;
	int m_fd;
	void expand_map(size_t req);

private:
	zmmap_stream();
	zmmap_stream(const zmmap_stream&);
};


}  // namespace server
}  // namespace kumo

#endif  /* server/zmmap_stream.h */

