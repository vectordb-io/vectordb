file demo
set pagination off
set print pretty

echo b epoll_wait\n
b epoll_wait

echo r\n
r

echo bt\n
bt