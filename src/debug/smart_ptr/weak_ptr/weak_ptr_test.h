#include <iostream>
#include <memory>

class AA {
 public:
  AA(int a) : aa(a) {}
  int aa;

  ~AA() { std::cout << "~AA()" << std::endl; }
};

void test() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  std::cout << "sizeof(void *): " << sizeof(void*) << std::endl;
}

void test2() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  std::shared_ptr<AA> sptr = std::make_shared<AA>(99);
  std::weak_ptr<AA> wptr = sptr;
  std::cout << "sizeof(wptr):" << sizeof(wptr) << std::endl;
  uint32_t uc = wptr.use_count();
  std::cout << "uc: " << uc << std::endl;
}

void test3() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  std::shared_ptr<AA> sptr = std::make_shared<AA>(99);
  std::shared_ptr<AA> sptr2 = sptr;  // use_count = 2
  std::weak_ptr<AA> wptr;
  wptr = sptr;  // weak_count = 2
  std::weak_ptr<AA> wptr2;
  wptr2 = sptr;  // weak_count = 3

  std::cout << "wptr.use_count(): " << wptr.use_count() << std::endl;
  std::cout << "wptr2.use_count(): " << wptr2.use_count() << std::endl;
  std::cout << "sptr.use_count(): " << sptr.use_count() << std::endl;
  std::cout << "sptr2.use_count(): " << sptr2.use_count() << std::endl;
}

void test4() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  std::shared_ptr<AA> sptr = std::make_shared<AA>(99);
  std::shared_ptr<AA> sptr2 = sptr;  // use_count = 2
  std::weak_ptr<AA> wptr;
  wptr = sptr;  // weak_count = 2

  {
    std::weak_ptr<AA> wptr2;
    wptr2 = sptr;  // weak_count = 3
  }

  std::cout << "***" << std::endl;
}

void test5() {
  std::cout << std::endl << "---------" << __func__ << std::endl;

  std::weak_ptr<AA> wptr;
  {
    std::shared_ptr<AA> sptr = std::make_shared<AA>(99);
    wptr = sptr;

    std::shared_ptr<AA> sptr2;
    sptr2 = wptr.lock();
    if (sptr2) {
      std::cout << sptr2->aa << std::endl;
    }
  }

  {
    std::shared_ptr<AA> sptr3;
    sptr3 = wptr.lock();
    if (sptr3) {
      std::cout << sptr3->aa << std::endl;
    } else {
      std::cout << "object released !!!" << std::endl;
    }
  }

  std::cout << "***" << std::endl;
}