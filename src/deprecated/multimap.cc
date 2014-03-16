#include <iostream>
#include <map>

using namespace std;

int main(int argc, const char *argv[])
{
 std::multimap<char, int> mm;
 mm.insert (make_pair<char, int> ('a', 10));
 mm.insert (make_pair<char, int> ('b', 10));

 for (auto it : mm) 
  cout << (*it).first << " ==> " << (*it).second << endl;
 
 return 0;
}
