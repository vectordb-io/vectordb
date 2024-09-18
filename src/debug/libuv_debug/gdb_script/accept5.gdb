file demo
set pagination off
set print pretty

echo b uv__io_start if w->fd==12\n
b uv__io_start if w->fd==12

echo r\n
r

echo p *w\n
p *w

echo bt\n
bt
