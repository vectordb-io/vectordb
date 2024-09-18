file demo
set pagination off
set print pretty

echo b src/unix/stream.c:891 if stream->io_watcher.fd==12\n
b src/unix/stream.c:891 if stream->io_watcher.fd==12

echo r\n
r

echo s\n
s

echo n\n
n

echo s\n
s

echo bt\n
bt