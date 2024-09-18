set pagination off
set print pretty

file size_test

b main
r

b std::__weak_ptr<AA, (__gnu_cxx::_Lock_policy)2>::use_count
c
s

p *this
p _M_ptr
p _M_refcount

c