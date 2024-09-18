set pagination off
set print pretty

file test_size

b main
r

b std::unique_ptr<AA, void (*)(AA*)>::get
c
bt
info args

p _M_t

# 如何拿到 std::unique_ptr<AA, void (*)(AA*)>::get
# 先用get行号打断点，s进去，从gdb中可以看到类型 std::unique_ptr<AA, void (*)(AA*)>::get