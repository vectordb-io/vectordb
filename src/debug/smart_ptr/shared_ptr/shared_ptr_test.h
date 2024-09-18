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
  // std::shared_ptr<AA> sptr(new AA(99));
  std::cout << "sizeof(sptr):" << sizeof(sptr) << std::endl;
  AA* p = sptr.get();
  printf("p:%p \n", p);
  std::cout << "p->aa:" << p->aa << std::endl;
}

void test3() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  { std::shared_ptr<AA> sptr = std::make_shared<AA>(99); }
  std::cout << "***" << std::endl;
}

void test4() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  { std::shared_ptr<AA> sptr(new AA(99)); }
  std::cout << "***" << std::endl;
}

void test5() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  {
    std::shared_ptr<AA> sptr(new AA(99));
    uint32_t uc = sptr.use_count();
    std::cout << "uc: " << uc << std::endl;
  }
  std::cout << "***" << std::endl;
}

void test6() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  {
    std::shared_ptr<AA> sptr(new AA(99));
    uint32_t uc = sptr.use_count();
    std::cout << "uc: " << uc << std::endl;

    std::shared_ptr<AA> sptr2;
    sptr2 = sptr;
    uc = sptr.use_count();
    std::cout << "uc: " << uc << std::endl;
  }
  std::cout << "***" << std::endl;
}

void test7() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  {
    std::shared_ptr<AA> sptr(new AA(99));
    uint32_t uc = sptr.use_count();
    std::cout << "uc: " << uc << std::endl;

    std::shared_ptr<AA> sptr2 = sptr;
    uc = sptr.use_count();
    std::cout << "uc: " << uc << std::endl;

    sptr = nullptr;
    uc = sptr2.use_count();
    std::cout << "uc: " << uc << std::endl;
  }
  std::cout << "***" << std::endl;
}

void test8() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  {
    std::shared_ptr<AA> sptr(new AA(99));
    uint32_t uc = sptr.use_count();
    std::cout << "uc: " << uc << std::endl;

    std::shared_ptr<AA> sptr2;
    sptr2 = sptr;
    uc = sptr.use_count();
    std::cout << "uc: " << uc << std::endl;

    sptr.reset();
    uc = sptr2.use_count();
    std::cout << "uc: " << uc << std::endl;
  }
  std::cout << "***" << std::endl;
}

void test9() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  {
    std::shared_ptr<AA> sptr(new AA(99));
    uint32_t uc = sptr.use_count();
    std::cout << "uc: " << uc << std::endl;

    sptr.reset();
    std::cout << "***" << std::endl;
  }
}

void test10() {
  std::cout << std::endl << "---------" << __func__ << std::endl;
  {
    std::shared_ptr<AA> sptr(new AA(99));
    uint32_t uc = sptr.use_count();
    std::cout << "uc: " << uc << std::endl;

    std::shared_ptr<AA> sptr2 = sptr;
    uc = sptr.use_count();
    std::cout << "uc: " << uc << std::endl;

    sptr = std::make_shared<AA>(88);
    uc = sptr2.use_count();
    std::cout << "uc: " << uc << std::endl;
  }
  std::cout << "***" << std::endl;
}

class BB : public std::enable_shared_from_this<BB> {
 public:
  BB(int b) : bb(b) {}
  int bb;

  ~BB() { std::cout << "~BB()" << std::endl; }
};

void test11() {
  try {
    BB x(5);
    std::shared_ptr<BB> sptr = x.shared_from_this();

  } catch (const std::exception& e) {
    std::cerr << "Standard exception: " << e.what() << std::endl;

  } catch (...) {
    std::cerr << "An unexpected error occurred." << std::endl;
  }

  {
    std::shared_ptr<BB> sptr;
    sptr = std::make_shared<BB>(6);
    std::shared_ptr<BB> sptr2 = sptr.get()->shared_from_this();

    std::cout << sptr->bb << std::endl;
    std::cout << sptr2->bb << std::endl;
  }
  std::cout << "***" << std::endl;
}

void test12() {
  {
    std::shared_ptr<BB> sptr;
    sptr = std::make_shared<BB>(6);
    std::shared_ptr<BB> sptr2 = sptr.get()->shared_from_this();

    std::cout << sptr->bb << std::endl;
    std::cout << sptr2->bb << std::endl;
  }
  std::cout << "***" << std::endl;
}