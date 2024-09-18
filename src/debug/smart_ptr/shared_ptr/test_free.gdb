set pagination off
set print pretty

file test_malloc

b main
r

b free

c
bt