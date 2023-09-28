#include <algorithm>
#include <bits/stdc++.h>
#include <list>

struct S
{
  int a;

  // bool operator<( const struct S& next ) const { return a < next.a; }
};

int main()
{
  std::list<struct S> list;
  list.emplace_back( ( struct S ) { 1 } );
  list.emplace_back( ( struct S ) { 2 } );
  list.emplace_back( ( struct S ) { 3 } );
  list.emplace_back( ( struct S ) { 4 } );
  // std::sort( list.begin(), list.end(), [&]( const auto& a, const auto& b ) { return a.a - b.a; } );
  list.sort([&]( const auto& a, const auto& b ) { return a.a - b.a; });
  for ( auto& item : list ) {
    std::cout << item.a << std::endl;
  }
  return 0;
}