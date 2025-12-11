#include <pthread.h>

#include <algorithm>
#include <iostream>
#include <vector>

using namespace std;

int
main()
{
  std::vector<int> v = {1, 2, 3, 4, 5};
  auto it = std::find(v.begin(), v.end(), 3);
  std::cout << v.size() << endl;
  std::cout << v.capacity() << endl;
  if (it != v.end()) {
    v.erase(it);
    std::cout << "after erase: ";
    for (auto& i : v) {
      std::cout << i << " ";
    }
    std::cout << endl;

    v.erase(it);

    for (auto& i : v) {
      std::cout << i << " ";
    }
    std::cout << endl;
  }
  std::cout << v.size() << endl;

  std::cout << v.capacity() << endl;

  return 0;
}