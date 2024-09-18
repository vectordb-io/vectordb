set pagination off
set print pretty

file weak_assign_shared

b main
r

b test5
c

n 3

echo "----------1111------------\n"
p wptr._M_ptr
p wptr._M_refcount

n 

# init sptr

echo "----------2222------------\n"
p sptr._M_ptr
p sptr._M_refcount
p *(sptr._M_refcount._M_pi)

echo "----------3333------------\n"
p wptr._M_ptr
p wptr._M_refcount


n 

# after wptr = sptr

echo "----------4444------------\n"
p sptr._M_ptr
p sptr._M_refcount
p *(sptr._M_refcount._M_pi)

echo "----------5555------------\n"
p wptr._M_ptr
p wptr._M_refcount
p *(wptr._M_refcount._M_pi)

n 

# after sptr destructor

echo "----------6666------------\n"
p wptr._M_ptr
p wptr._M_refcount
p *(wptr._M_refcount._M_pi)
