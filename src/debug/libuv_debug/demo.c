#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uv.h"

typedef struct server_t {
  uv_loop_t loop;
  uv_timer_t timer;
  uv_tcp_t acceptor;
  uv_tcp_t *server_conn;
  uv_tcp_t *client_conn;
  uv_async_t *async;
} server_t;

typedef struct timer_param_t {
  int counter;
  server_t *server;
  char send_flag;
} timer_param_t;

void async_close_cb(uv_handle_t *handle) {
  fprintf(stderr, "async_close_cb \n");
  free(handle);
}

void server_conn_close_cb(uv_handle_t *handle) {
  fprintf(stderr, "server_conn_close_cb \n");
  free(handle);
}

void client_conn_close_cb(uv_handle_t *handle) {
  fprintf(stderr, "client_conn_close_cb \n");
  free(handle);
}

void acceptor_close_cb(uv_handle_t *handle) {
  fprintf(stderr, "acceptor_close_cb \n");
}

void timer_close_cb(uv_handle_t *handle) {
  fprintf(stderr, "timer_close_cb %d \n",
          ((timer_param_t *)(handle->data))->counter);
  free(handle->data);
}

void close_server(server_t *server) {
  uv_close((uv_handle_t *)&server->acceptor, acceptor_close_cb);
  uv_close((uv_handle_t *)&server->timer, timer_close_cb);

  if (server->server_conn != NULL) {
    uv_close((uv_handle_t *)(server->server_conn), server_conn_close_cb);
    server->server_conn = NULL;
  }

  if (server->client_conn != NULL) {
    uv_close((uv_handle_t *)(server->client_conn), client_conn_close_cb);
    server->client_conn = NULL;
  }

  if (server->async != NULL) {
    uv_close((uv_handle_t *)(server->async), async_close_cb);
    server->async = NULL;
  }
}

void finish_write(uv_write_t *req, int status) {
  if (status) {
    fprintf(stderr, "Write error %s\n", uv_strerror(status));
  }
  free(((uv_buf_t *)(req->data))->base);
  free(((uv_buf_t *)(req->data)));
  free(req);
}

void async_cb(uv_async_t *handle) { fprintf(stderr, "async_cb \n"); }

#define TIMER_COUNTER 1000000

void timer_cb(uv_timer_t *handle) {
  fprintf(stderr, "timer_cb %d \n", ((timer_param_t *)(handle->data))->counter);
  ((timer_param_t *)(handle->data))->counter++;

  if (((timer_param_t *)(handle->data))->server->client_conn != NULL &&
      uv_is_active((uv_handle_t *)(((timer_param_t *)(handle->data))
                                       ->server->client_conn))) {
    // write message
    uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
    req->data = malloc(sizeof(uv_buf_t));
    ((uv_buf_t *)(req->data))->base = malloc(6);
    ((uv_buf_t *)(req->data))->len = 6;
    strcpy(((uv_buf_t *)(req->data))->base, "hello");
    uv_write(
        (uv_write_t *)req,
        (uv_stream_t *)(((timer_param_t *)(handle->data))->server->client_conn),
        (uv_buf_t *)(req->data), 1, finish_write);
  }

  if (((timer_param_t *)(handle->data))->server->async != NULL &&
      uv_is_active(
          (uv_handle_t *)(((timer_param_t *)(handle->data))->server->async))) {
    uv_async_send((((timer_param_t *)(handle->data))->server->async));
  }

  if (((timer_param_t *)(handle->data))->counter == TIMER_COUNTER) {
    fprintf(stderr, "timer stop \n");
    uv_timer_stop(handle);
    close_server(((timer_param_t *)(handle->data))->server);
  }
}

void on_message(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
  server_t *server = (server_t *)(stream->data);

  if (nread > 0) {
    fprintf(stderr, "%p recv [%s] \n", stream, buf->base);

    if (stream == (uv_stream_t *)(server->server_conn)) {
      uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
      req->data = malloc(sizeof(uv_buf_t));
      ((uv_buf_t *)(req->data))->base = buf->base;
      ((uv_buf_t *)(req->data))->len = nread;
      uv_write((uv_write_t *)req, stream, (uv_buf_t *)(req->data), 1,
               finish_write);
      return;

    } else {
      free(buf->base);
      return;
    }
  }
  if (nread < 0) {
    if (nread != UV_EOF) fprintf(stderr, "Read error %s\n", uv_err_name(nread));
    uv_close((uv_handle_t *)stream, server_conn_close_cb);
  }

  free(buf->base);
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = (char *)malloc(suggested_size);
  buf->len = suggested_size;
}

void on_new_connection(uv_stream_t *acceptor, int status) {
  server_t *server = (server_t *)(acceptor->data);
  if (status < 0) {
    fprintf(stderr, "new connection error %s\n", uv_strerror(status));
    return;
  }

  server->server_conn = malloc(sizeof(uv_tcp_t));
  server->server_conn->data = server;
  uv_tcp_init(&server->loop, server->server_conn);
  if (uv_accept(acceptor, (uv_stream_t *)(server->server_conn)) == 0) {
    uv_read_start((uv_stream_t *)(server->server_conn), alloc_buffer,
                  on_message);
  } else {
    close_server(server);
  }
}

void on_client_connect(uv_connect_t *req, int status) {
  if (status < 0) {
    fprintf(stderr, "connect failed error %s\n", uv_err_name(status));
    free(req);
    return;
  }

  server_t *server = (server_t *)(req->data);

  uv_read_start((uv_stream_t *)(server->client_conn), alloc_buffer, on_message);

  free(req);
}

int main() {
  server_t server;
  server.server_conn = NULL;
  server.client_conn = NULL;
  uv_loop_init(&server.loop);

  // timer
  server.timer.data = malloc(sizeof(timer_param_t));
  ((timer_param_t *)(server.timer.data))->counter = 0;
  ((timer_param_t *)(server.timer.data))->server = &server;
  ((timer_param_t *)(server.timer.data))->send_flag = 1;
  uv_timer_init(&server.loop, &server.timer);
  uv_timer_start(&server.timer, timer_cb, 0, 1000);

  // acceptor
  struct sockaddr_in addr;
  uv_tcp_init(&server.loop, &server.acceptor);
  server.acceptor.data = &server;
  uv_ip4_addr("0.0.0.0", 9999, &addr);

  uv_tcp_bind(&server.acceptor, (const struct sockaddr *)&addr, 0);
  int rv = uv_listen((uv_stream_t *)&server.acceptor, 128, on_new_connection);
  if (rv == 0) {
    fprintf(stderr, "listening on 0.0.0.0:9999 \n");
  } else {
    fprintf(stderr, "rv = %d, %s \n", rv, uv_strerror(rv));
    exit(-1);
  }

  // client_conn
  server.client_conn = malloc(sizeof(uv_tcp_t));
  server.client_conn->data = &server;
  uv_tcp_init(&server.loop, server.client_conn);
  uv_connect_t *connect_req = (uv_connect_t *)malloc(sizeof(uv_connect_t));
  connect_req->data = &server;
  uv_tcp_connect(connect_req, server.client_conn,
                 (const struct sockaddr *)&addr, on_client_connect);

  // async
  server.async = malloc(sizeof(uv_async_t));
  server.async->data = &server;
  uv_async_init(&server.loop, server.async, async_cb);

  // run
  uv_run(&server.loop, UV_RUN_DEFAULT);
  fprintf(stderr, "quitting... \n");

  uv_loop_close(&server.loop);
  return 0;
}