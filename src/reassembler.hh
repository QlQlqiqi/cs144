#pragma once

#include "byte_stream.hh"

#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <utility>

class Reassembler
{
private:
  // 下一个被 push 的 index
  uint64_t next_index_ { 0 };
  // reassembler 内部存储的数据
  std::list<std::pair<uint64_t, std::string>> list_ {};
  // reassembler 存储的数据量
  uint64_t pending_types_ { 0 };
  // 是否收到 end signal
  bool is_end_ { false };
  // stream 最后的 byte 的 next index
  uint64_t end_index_ { 0 };

  // 内部调用的 insert
  // 修改 next_index_, first_idex, data
  void Insert( uint64_t& first_index, std::string& data, Writer& output );

public:
  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring, Writer& output );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;
};
