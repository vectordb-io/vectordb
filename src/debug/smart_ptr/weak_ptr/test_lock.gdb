set pagination off
set print pretty

file test_lock

b main
r

b test5
c
n 5

echo "----------1111------------\n"
p sptr._M_ptr
p sptr._M_refcount
p *(sptr._M_refcount._M_pi)

echo "----------2222------------\n"
p wptr._M_ptr
p wptr._M_refcount
p *(wptr._M_refcount._M_pi)



b std::weak_ptr<AA>::lock
c

s
bt

s
bt

s
bt

s
bt

n 7

echo "----------4444------------\n"
p sptr2._M_ptr
p sptr2._M_refcount
p *(sptr2._M_refcount._M_pi)

echo "----------5555------------\n"
p wptr._M_ptr
p wptr._M_refcount
p *(wptr._M_refcount._M_pi)

echo "----------6666------------\n"
p sptr._M_ptr
p sptr._M_refcount
p *(sptr._M_refcount._M_pi)