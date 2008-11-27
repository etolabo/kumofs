#include "rpc/transport.h"

namespace rpc {


void basic_transport::send_data(int fd, const char* buf, size_t buflen, void (*finalize)(void*), void* data)
{
	mp::iothreads::send_data(fd, buf, buflen, finalize, data, true);
}

void basic_transport::send_datav(int fd, vrefbuffer* buf, void (*finalize)(void*), void* data)
{
	size_t sz = buf->vector_size();
	struct iovec vb[sz];
	mp::iothreads::writer::reqvec vr[sz];

	buf->get_vector(vb);
	for(size_t i=0; i < sz-1; ++i) {
		vr[i] = mp::iothreads::writer::reqvec(
				&mp::iothreads::writer::finalize_nothing, NULL);
	}
	vr[sz-1] = mp::iothreads::writer::reqvec(finalize, data, true);

	mp::iothreads::send_datav(fd, vb, vr, sz);
}


}  // namespace rpc

