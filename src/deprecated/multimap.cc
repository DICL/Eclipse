#include <iostream>
#include <map>

using namespace std;

int main(int argc, const char *argv[])
{

 std::multimap<char, int> mm;
 std::multimap<char, int>::iterator it;

 mm.insert (std::pair<char, int> ('a', 10));
 mm.insert (std::pair<char, int> ('b', 10));

 for (it = mm.begin(); it != mm.end(); it++) 
  cout << (*it).first << " ==> " << (*it).second << endl;
 
 return 0;
}
