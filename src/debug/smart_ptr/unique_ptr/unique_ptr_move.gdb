set pagination off
set print pretty

file unique_ptr_move

b main
r

b std::unique_ptr<AA, std::default_delete<AA> >::operator=
c
bt
info args