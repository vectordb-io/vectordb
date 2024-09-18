set pagination off
set print pretty

file test_weak_destroy

b main
r

b std::_Sp_counted_ptr_inplace<AA, std::allocator<AA>, (__gnu_cxx::_Lock_policy)2>::_M_destroy

c
bt


