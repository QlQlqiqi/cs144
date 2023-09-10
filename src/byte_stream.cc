#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(uint64_t capacity) : capacity_(capacity), buffer_() {}

void Writer::push(string data) {
  uint64_t sz = std::min(data.size(), capacity_ - len_);
  for (uint64_t i = 0; i < sz; i++) {
    buffer_.emplace(data.at(i));
  }
  len_ += sz;
  written_bytes_ += sz;
}

void Writer::close() { is_closed_ = true; }

void Writer::set_error() { is_error_ = true; }

bool Writer::is_closed() const { return is_closed_; }

uint64_t Writer::available_capacity() const { return capacity_ - len_; }

uint64_t Writer::bytes_pushed() const { return written_bytes_; }

string_view Reader::peek() const { return {&buffer_.front(), 1}; }

bool Reader::is_finished() const { return is_closed_ && len_ == 0; }

bool Reader::has_error() const { return is_error_; }

void Reader::pop(uint64_t len) {
  uint64_t sz = std::min(len, len_);
  while (sz-- > 0) {
    buffer_.pop();
  }
  len_ -= sz;
  read_bytes_ += sz;
}

uint64_t Reader::bytes_buffered() const { return len_; }

uint64_t Reader::bytes_popped() const { return read_bytes_; }
