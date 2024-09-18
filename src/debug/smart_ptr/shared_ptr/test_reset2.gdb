set pagination off
set print pretty

file test_reset

b main
r

b AA::~AA

c
bt
