set pagination off
set print pretty

file test_shared_from_this2

b main
r

b std::enable_shared_from_this<BB>::_M_weak_assign<BB>

c
bt
