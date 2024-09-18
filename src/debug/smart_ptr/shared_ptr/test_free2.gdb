set pagination off
set print pretty

file test_malloc2

b main
r

b free

c
bt

c
bt