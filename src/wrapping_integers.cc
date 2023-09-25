#include "wrapping_integers.hh"
#include <cstdint>
#include <iostream>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // 4294967296 == 2^32
  return Wrap32( n + zero_point.raw_value_ );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // 计算出当前 absolute index
  uint64_t now_abs = raw_value_ - zero_point.raw_value_;
  // 如果 checkpoint < absolute index
  if ( checkpoint < now_abs ) {
    // 如果 absolute index 在 checkpoint 的范围内
    if ( now_abs - checkpoint <= ( 1UL << 31 ) ) {
      return now_abs;
    }
    // 否则令 checkpoint > absolute index
    checkpoint |= ( 1UL << 32 );
  }

  // absolute index <= checkpoint
  // 让 absolute index 在 checkpoint 的范围内
  uint64_t div = ( checkpoint - now_abs ) / ( 1UL << 32 );
  now_abs += div * ( 1UL << 32 );
  // 如果 absolute index 在 checkpoint 的范围内，直接返回
  // 否则给 absolute index 增加一个周期，让其在 checkpoint 的范围内
  if ( checkpoint - now_abs <= ( 1UL << 31 ) ) {
    return now_abs;
  }
  return now_abs += ( 1UL << 32 );
}
