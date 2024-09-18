set pagination off
set print pretty

file test_release

b std::unique_ptr<AA, std::default_delete<AA> >::release
r
bt

p __p
p _M_t
