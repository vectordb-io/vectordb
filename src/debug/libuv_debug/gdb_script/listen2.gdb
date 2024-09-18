file demo
set pagination off
set print pretty

echo b uv_tcp_listen\n
b uv_tcp_listen

echo r\n
r

echo b 359\n
b 359

echo p tcp->io_watcher.fd\n
p tcp->io_watcher.fd

echo c\n
c

echo b 366\n
b 366

echo c\n
c

echo n\n
n

echo p tcp->io_watcher\n
p tcp->io_watcher

echo s\n
s

echo b 900\n
b 900

echo c\n
c

echo n\n
n 

echo n\n
n 

echo n\n
n 

echo n\n
n 

echo p w->fd\n
p w->fd

echo p *(loop->watchers[w->fd])\n
p *(loop->watchers[w->fd])

