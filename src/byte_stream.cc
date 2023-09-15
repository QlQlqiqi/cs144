#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  uint64_t data_sz = data.size();
  uint64_t sz = std::min( data_sz, available_capacity() );
  if ( sz == 0 ) {
    return;
  }
  if ( sz < data_sz ) {
    data.resize( sz );
  }
  bool changed = buffer_.empty();
  buffer_.emplace( data );
  if ( changed ) {
    buffer_first_ = buffer_.front();
  }
  written_bytes_ += sz;
  // std::cout << "push " << sz << ": " << data << std::endl;
}

void Writer::close()
{
  is_closed_ = true;
}

void Writer::set_error()
{
  is_error_ = true;
}

bool Writer::is_closed() const
{
  return is_closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - reader().bytes_buffered();
}

uint64_t Writer::bytes_pushed() const
{
  return written_bytes_;
}

string_view Reader::peek() const
{
  return buffer_first_;
}

bool Reader::is_finished() const
{
  return is_closed_ && bytes_buffered() == 0;
}

bool Reader::has_error() const
{
  return is_error_;
}

void Reader::pop( uint64_t len )
{
  read_bytes_ += len;
  while ( len > 0 && !buffer_.empty() ) {
    uint64_t sz = buffer_first_.size();
    if ( len >= sz ) {
      len -= sz;
      buffer_.pop();
      buffer_first_ = buffer_.front();
    } else {
      buffer_first_.remove_prefix( len );
      len = 0;
    }
  }
  read_bytes_ -= len;
}

uint64_t Reader::bytes_buffered() const
{
  return writer().bytes_pushed() - bytes_popped();
}

uint64_t Reader::bytes_popped() const
{
  return read_bytes_;
}
