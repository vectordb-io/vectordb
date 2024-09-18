file demo
set pagination off
set print pretty

echo b linux-core.c:252 if w->fd==10\n
b linux-core.c:252 if w->fd==10

echo r\n
r

echo bt\n
bt