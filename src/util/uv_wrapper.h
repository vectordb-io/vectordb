#ifndef VRAFT_UV_WRAPPER_H_
#define VRAFT_UV_WRAPPER_H_

#include "util.h"
#include "uv.h"
#include "vraft_logger.h"

namespace vraft {

using UvLoop = uv_loop_t;
using UvTcp = uv_tcp_t;
using UvConnect = uv_connect_t;
using UvStream = uv_stream_t;
using UvHandle = uv_handle_t;
using UvWrite = uv_write_t;
using UvBuf = uv_buf_t;
using UvAsync = uv_async_t;
using UvTimer = uv_timer_t;
using UvRunMode = uv_run_mode;

using UvConnectionCb = uv_connection_cb;
using UvConnectCb = uv_connect_cb;
using UvAllocCb = uv_alloc_cb;
using UvReadCb = uv_read_cb;
using UvCloseCb = uv_close_cb;
using UvWriteCb = uv_write_cb;
using UvAsyncCb = uv_async_cb;
using UvTimerCb = uv_timer_cb;

inline const char *UvStrError(int err) {
  if (err == 0) {
    return "ok";
  } else {
    return ::uv_strerror(err);
  }
}

inline int UvIsActive(const UvHandle *handle) { return ::uv_is_active(handle); }

inline int UvIsClosing(const UvHandle *handle) {
  return ::uv_is_closing(handle);
}

inline int UvLoopInit(UvLoop *loop) {
  int rv = ::uv_loop_init(loop);
  vraft_logger.FTrace("uv_loop_init:%p, rv:%d, %s", loop, rv, UvStrError(rv));
  return rv;
}

inline int UvRun(UvLoop *loop, UvRunMode mode) {
  int rv = ::uv_run(loop, mode);
  vraft_logger.FTrace("uv_run:%p, rv:%d, %s", loop, rv, UvStrError(rv));
  return rv;
}

inline void UvStop(UvLoop *loop) {
  ::uv_stop(loop);
  vraft_logger.FTrace("uv_stop:%p", loop);
}

inline int UvLoopClose(UvLoop *loop) {
  int rv = ::uv_loop_close(loop);
  vraft_logger.FTrace("uv_loop_close:%p, rv:%d, %s", loop, rv, UvStrError(rv));
  return rv;
}

inline int UvLoopAlive(const UvLoop *loop) { return ::uv_loop_alive(loop); }

inline int UvTcpInit(UvLoop *loop, UvTcp *tcp) {
  int rv = ::uv_tcp_init(loop, tcp);
  vraft_logger.FTrace("uv_tcp_init:%p, loop:%p, rv:%d, %s", tcp, loop, rv,
                      UvStrError(rv));
  return rv;
}

inline int UvTcpBind(UvTcp *handle, const struct sockaddr *addr,
                     unsigned int flags) {
  int rv = ::uv_tcp_bind(handle, addr, flags);
  vraft_logger.FTrace("uv_tcp_bind:%p, rv:%d, %s", handle, rv, UvStrError(rv));
  return rv;
}

inline int UvListen(UvStream *stream, int backlog, UvConnectionCb cb) {
  int rv = ::uv_listen(stream, backlog, cb);
  vraft_logger.FTrace("uv_listen:%p, rv:%d, %s", stream, rv, UvStrError(rv));
  return rv;
}

inline int UvAccept(UvStream *server, UvStream *client) {
  int rv = ::uv_accept(server, client);
  vraft_logger.FTrace("uv_accept:%p, client:%p, rv:%d, %s", server, client, rv,
                      UvStrError(rv));
  return rv;
}

inline int UvTcpConnect(UvConnect *req, UvTcp *handle,
                        const struct sockaddr *addr, UvConnectCb cb) {
  int rv = ::uv_tcp_connect(req, handle, addr, cb);
  vraft_logger.FTrace("uv_tcp_connect:%p, active:%d, req:%p, rv:%d, %s", handle,
                      UvIsActive(reinterpret_cast<UvHandle *>(handle)), req, rv,
                      UvStrError(rv));
  return rv;
}

inline int UvTcpNodelay(UvTcp *handle, int enable) {
  int rv = ::uv_tcp_nodelay(handle, enable);
  vraft_logger.FTrace("uv_tcp_nodelay:%p, nodelay:%d, rv:%d, %s", handle,
                      enable, rv, UvStrError(rv));
  return rv;
}

inline int UvReadStart(UvStream *uv_stream, UvAllocCb alloc_cb,
                       UvReadCb read_cb) {
  int rv = ::uv_read_start(uv_stream, alloc_cb, read_cb);
  vraft_logger.FTrace("uv_read_start:%p, active:%d, rv:%d, %s", uv_stream,
                      UvIsActive(reinterpret_cast<UvHandle *>(uv_stream)), rv,
                      UvStrError(rv));
  return rv;
}

inline void UvClose(UvHandle *handle, UvCloseCb close_cb) {
  ::uv_close(handle, close_cb);
  vraft_logger.FTrace("uv_close:%p, active:%d", handle, UvIsActive(handle));
}

inline UvBuf UvBufInit(char *base, unsigned int len) {
  return ::uv_buf_init(base, len);
}

// use UvWrite2 to avoid conflicting with uv_write_t
inline int UvWrite2(UvWrite *req, UvStream *handle, const uv_buf_t bufs[],
                    unsigned int nbufs, UvWriteCb cb) {
  int rv = ::uv_write(req, handle, bufs, nbufs, cb);

  uint32_t u32 = 0;
  int bytes = 0;
  for (unsigned int i = 0; i < nbufs; ++i) {
    if (bufs[i].base != nullptr) {
      bytes += bufs[i].len;
      u32 += Crc32(bufs[i].base, bufs[i].len);
    }
  }

  vraft_logger.FTrace(
      "uv_write:%p, req:%p, bytes:%d, check:%X, nbufs:%d, active:%d, rv:%d, %s",
      handle, req, bytes, u32, nbufs,
      UvIsActive(reinterpret_cast<UvHandle *>(handle)), rv, UvStrError(rv));

  for (unsigned int i = 0; i < nbufs; ++i) {
    vraft_logger.FDebug("send data:%s",
                        StrToHexStr(bufs[i].base, bufs[i].len).c_str());
  }

  return rv;
}

inline int UvAsyncInit(UvLoop *loop, UvAsync *async, UvAsyncCb cb) {
  int rv = ::uv_async_init(loop, async, cb);
  vraft_logger.FTrace("uv_async_init:%p, loop:%p, rv:%d, %s", async, loop, rv,
                      UvStrError(rv));
  return rv;
}

inline int UvAsyncSend(UvAsync *async) {
  int rv = ::uv_async_send(async);
  vraft_logger.FTrace("uv_async_send:%p, rv:%d, %s", async, rv, UvStrError(rv));
  return rv;
}

inline int UvTimerInit(UvLoop *loop, UvTimer *handle) {
  int rv = ::uv_timer_init(loop, handle);
  vraft_logger.FTrace("uv_timer_init:%p, loop:%p, rv:%d, %s", handle, loop, rv,
                      UvStrError(rv));
  return rv;
}

inline int UvimerStart(UvTimer *handle, UvTimerCb cb, uint64_t timeout,
                       uint64_t repeat) {
  int rv = ::uv_timer_start(handle, cb, timeout, repeat);
  vraft_logger.FTrace("uv_timer_start:%p, active:%d, rv:%d, %s", handle,
                      UvIsActive(reinterpret_cast<UvHandle *>(handle)), rv,
                      UvStrError(rv));
  return rv;
}

inline int UvTimerStop(UvTimer *handle) {
  int rv = ::uv_timer_stop(handle);
  vraft_logger.FTrace("uv_timer_stop:%p, active:%d, rv:%d, %s", handle,
                      UvIsActive(reinterpret_cast<UvHandle *>(handle)), rv,
                      UvStrError(rv));
  return rv;
}

inline int UvTimerAgain(UvTimer *handle) {
  int rv = ::uv_timer_again(handle);
  vraft_logger.FTrace("uv_timer_again:%p, active:%d, rv:%d, %s", handle,
                      UvIsActive(reinterpret_cast<UvHandle *>(handle)), rv,
                      UvStrError(rv));
  return rv;
}

inline int UvIsReadable(const UvStream *stream) {
  return ::uv_is_readable(stream);
}

inline int UvIsWritable(const UvStream *stream) {
  return ::uv_is_writable(stream);
}

inline int UvTcpGetSockName(const UvTcp *handle, struct sockaddr *name,
                            int *namelen) {
  return ::uv_tcp_getsockname(handle, name, namelen);
}

inline int UvTcpGetPeerName(const UvTcp *handle, struct sockaddr *name,
                            int *namelen) {
  return ::uv_tcp_getpeername(handle, name, namelen);
}

}  // namespace vraft

#endif
