set pagination off
set print pretty

file weak_assign_shared

b main
r

b test5
c

n 3

