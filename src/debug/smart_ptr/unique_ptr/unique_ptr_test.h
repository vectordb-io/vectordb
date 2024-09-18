#include <iostream>
#include <memory>

class AA {
 public:
  AA(int a) : aa(a), bb(a + 1), cc(a + 2) {
    std::cout << "AA(" << a << ")" << std::endl;
  }
  ~AA() { std::cout << "~AA()" << std::endl; }

  int aa;
  int bb;
  int cc;
};

void test() {
  std::cout << std::endl << "---func:" << __func__ << std::endl;
  { std::unique_ptr<AA> uptr = std::make_unique<AA>(3); }
  std::cout << "******" << std::endl;
}

void test2() {
  std::cout << std::endl << "---func:" << __func__ << std::endl;
  std::unique_ptr<AA> uptr = std::make_unique<AA>(3);
  uptr.release();
  std::cout << "******" << std::endl;
}

void test3() {
  std::cout << std::endl << "---func:" << __func__ << std::endl;
  {
    std::unique_ptr<AA> uptr = std::make_unique<AA>(3);
    uptr.reset();
    std::cout << "******" << std::endl;
  }
}

void test4() {
  std::cout << std::endl << "---func:" << __func__ << std::endl;
  std::cout << "sizeof(void *): " << sizeof(void *) << std::endl;
}

void test5() {
  std::cout << std::endl << "---func:" << __func__ << std::endl;
  std::unique_ptr<AA> uptr = std::make_unique<AA>(3);
  std::cout << "sizeof(uptr): " << sizeof(uptr) << std::endl;
}

void MyDeleter(AA *p) { std::cout << "MyDeleter: p=" << p << std::endl; }

void test6() {
  std::cout << std::endl << "---func:" << __func__ << std::endl;
  std::unique_ptr<AA, decltype(MyDeleter) *> uptr(new AA(3), MyDeleter);
  std::cout << "uptr.get(): " << uptr.get() << std::endl;
  std::cout << "sizeof(uptr): " << sizeof(uptr) << std::endl;
}

void f(int x, int y) {
  x++;
  y++;
};

void test7() {
  std::cout << std::endl << "---func:" << __func__ << std::endl;
  std::unique_ptr<AA> uptr2;
  {
    std::unique_ptr<AA> uptr = std::make_unique<AA>(3);
    uptr2 = std::move(uptr);
  }
  std::cout << "******" << std::endl;
}