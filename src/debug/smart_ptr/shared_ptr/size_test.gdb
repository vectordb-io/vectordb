set pagination off
set print pretty

file size_test

b main
r

b std::__shared_ptr<AA, (__gnu_cxx::_Lock_policy)2>::get
c
s

echo p *this\n
p *this

echo p _M_ptr\n
p _M_ptr

echo p _M_refcount\n
p _M_refcount

echo p _M_refcount._M_pi->_M_use_count
p _M_refcount._M_pi->_M_use_count

echo p _M_refcount._M_pi->_M_weak_count
p _M_refcount._M_pi->_M_weak_count