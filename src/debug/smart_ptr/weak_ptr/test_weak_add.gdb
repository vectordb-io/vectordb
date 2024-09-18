set pagination off
set print pretty

file test_weak_add

b main
r

b std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_weak_add_ref

c
bt


