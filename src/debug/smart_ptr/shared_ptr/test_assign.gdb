set pagination off
set print pretty

file test_assign

b main
r

b std::__shared_count<(__gnu_cxx::_Lock_policy)2>::operator=
c
bt

b __atomic_add_dispatch
c
bt
