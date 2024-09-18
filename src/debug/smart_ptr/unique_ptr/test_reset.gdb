set pagination off
set print pretty

file test_reset

b main
r

b free
c
bt
info args

f 2
p __p