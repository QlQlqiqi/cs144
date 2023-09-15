#include "reassembler.hh"
#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <utility>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // 确定 stream end index
  if ( is_last_substring ) {
    is_end_ = true;
    end_index_ = max( end_index_, first_index + data.size() );
    // std::cout << end_index_ << std::endl;
  }

  while ( true ) {
    Insert( first_index, data, output );
    if ( is_end_ && next_index_ == end_index_ ) {
      output.close();
    }
    // 如果 data 为空，说明 Insert 有可能还能接收新的 data
    if ( data.empty() ) {
      // list 为空，则结束
      if ( list_.empty() ) {
        return;
      }
      auto next = list_.front();
      first_index = next.first;
      data = next.second;
      list_.pop_front();
      pending_types_ -= data.size();
      // std::cout << "delete \"" << data << "\" from list " << pending_types_ << std::endl;
      continue;
    }
    break;
  }

  // 否则，将 (first_index, data) 加入到 list_ 中，并更新 list_，避免 overlapping slice
  for ( auto it = list_.begin(); it != list_.end(); ) {
    auto idx = it->first;
    auto str = it->second;
    auto end_idx_data = first_index + data.size() - 1;
    auto end_idx_str = idx + str.size() - 1;
    // 1. 如果 data 在 str 之前，且没有重叠，则将 data 加入 list
    if ( end_idx_data < idx ) {
      list_.insert( it, { first_index, data } );
      pending_types_ += data.size();
      // std::cout << "insert \"" << data << "\" into list " << pending_types_ << std::endl;
      data.clear();
      break;
    }
    // 2. 如果 data 在 str 之前，但有重叠，则将 data 和 str 进行合并
    if ( first_index <= idx && end_idx_data + 1 >= idx ) {
      // 删除 str，并将 str 多出 data 的部分加在 data 之后
      it = list_.erase( it );
      pending_types_ -= str.size();
      // std::cout << "delete \"" << str << "\" from list " << pending_types_ << std::endl;
      data.append( str.substr( std::min( str.size(), end_idx_data + 1 - idx ) ) );
      continue;
    }
    // 3. 如果 str 完全在 data 之前，则 continue
    if ( end_idx_str < first_index ) {
      it++;
      continue;
    }
    // 4. 如果 str 在 data 之前，但有重叠，则将 data 和 str 进行合并
    // if ( idx <= first_index && end_idx_str + 1 >= first_index ) {
    // 删除 str，并将 data 多出 str 的部分加在 str 之后，
    // 并将新的 str 作为 data 重新进行合并，并修改 first index
    it = list_.erase( it );
    pending_types_ -= str.size();
    // std::cout << "delete \"" << str << "\" from list " << pending_types_ << std::endl;
    str.append( data.substr( std::min( data.size(), end_idx_str + 1 - first_index ) ) );
    data = str;
    first_index = idx;
    // continue;
    // }
    // std::cout << "Insert error" << std::endl;
    // throw exception();
  }

  if ( !data.empty() ) {
    // std::cout << output.available_capacity() << " " << first_index << " " << next_index_ << " " << data.size()
    //           << std::endl;
    auto len = std::min( output.available_capacity() - ( first_index - next_index_ ), data.size() );
    data.resize( len );
    list_.emplace_back( first_index, data );
    pending_types_ += len;
    // std::cout << "insert \"" << data << "\" into list " << pending_types_ << std::endl;
  }
}

void Reassembler::Insert( uint64_t& first_index, std::string& data, Writer& output )
{
  // std::cout << "Insert " << first_index << " " << data << " " << next_index_ << std::endl;
  // auto idx = first_index;
  // auto str = data;
  // 无法插入
  if ( first_index > next_index_ ) {
    return;
  }
  // 移除冗余的前缀
  if ( first_index < next_index_ ) {
    uint64_t gap = std::min( next_index_ - first_index, data.size() );
    first_index += gap;
    data = data.substr( gap );
  }
  uint64_t len = std::min( output.available_capacity(), data.size() );
  // std::cout << len << std::endl;
  output.push( data );
  next_index_ = std::max( next_index_, first_index + len );
  first_index += len;
  data = data.substr( len );
}

uint64_t Reassembler::bytes_pending() const
{
  return pending_types_;
}
