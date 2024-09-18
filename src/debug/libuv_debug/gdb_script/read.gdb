file demo
set pagination off
set print pretty

echo b src/unix/stream.c:1155 if stream->io_watcher.fd==12\n
b src/unix/stream.c:1153 if stream->io_watcher.fd==12

echo r\n
r

n
s

echo bt\n
bt
