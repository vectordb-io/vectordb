file demo
set pagination off
set print pretty

echo b connect\n
b connect

echo r\n
r

echo bt\n
bt

echo f 1\n
f 1

echo p handle->io_watcher.fd\n
p handle->io_watcher.fd