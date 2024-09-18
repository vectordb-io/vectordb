file demo
set pagination off
set print pretty

echo b epoll_create\n
b epoll_create

echo b epoll_create1\n
b epoll_create1


echo r\n
r

echo bt\n
bt

