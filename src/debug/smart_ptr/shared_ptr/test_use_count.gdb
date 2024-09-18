set pagination off
set print pretty

file test_use_count

b main
r

b std::_Sp_counted_base<(__gnu_cxx::_Lock_policy)2>::_M_get_use_count

c
bt
