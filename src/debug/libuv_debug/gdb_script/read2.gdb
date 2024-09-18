file demo
set pagination off
set print pretty

echo b src/unix/stream.c:1290 if stream->io_watcher.fd==12\n
b src/unix/stream.c:1290 if stream->io_watcher.fd==12

echo r\n
r

echo n\n
n

echo p *stream\n
p *stream

echo bt\n
bt