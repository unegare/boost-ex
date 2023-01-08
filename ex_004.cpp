#include <iostream>

#include <boost/coroutine2/coroutine.hpp>

using namespace boost::coroutines2;

void push_func_0(coroutine<int>::push_type &yield, int a) {
  for (int i = 0; i < a; i++) {
    yield(i);
  }
}

void test_push_func_0() {
  coroutine<int>::pull_type resume(std::bind(push_func_0, std::placeholders::_1, 6));
  while(resume) {
    std::cout << "got: " << resume.get() << std::endl;
    resume();
  }

  std::cout << "-------------------------" << std::endl;
  
  coroutine<int>::pull_type resume2(std::bind(push_func_0, std::placeholders::_1, 6));
  for (auto x : resume2) {
    std::cout << "iter: " << x << std::endl;
  }
}

void pull_func_1(coroutine<int>::pull_type &yield) {
  int round = 0;
  while (yield) {
    int x = yield.get();
    std::cout << "round: " << round << " : x : " << x << std::endl;
    yield();
    round++;
  }
}

void test_pull_func_1() {
  coroutine<int>::push_type sink(pull_func_1);
  std::vector<int> v{100,110,120,130,140,150};
  for (auto x : v) {
    sink(x);
  }
}

int main() {
  test_push_func_0();
  test_pull_func_1();
  return 0;
}
