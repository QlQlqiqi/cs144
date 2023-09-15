#include "tcp_receiver.hh"
#include "tcp_receiver_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <iostream>
#include <optional>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // 还未开始
  if ( !syn_ && !message.SYN ) {
    // std::cout << "dont start" << std::endl;
    return;
  }

  // 开始
  if ( message.SYN ) {
    syn_ = message.SYN;
    isn_ = message.seqno;
    message.seqno = message.seqno + 1;
  }

  // 结束
  fin_ = fin_ || message.FIN;

  // 这里的 idx 是以 tcp 中的 absolute index 为准
  // first index 是以 reassembler 中的 index 为准
  auto first_index = message.seqno.unwrap( isn_, inbound_stream.bytes_pushed() + 1 ) - 1;
  // std::cout << "first_index: " << first_index << std::endl;

  reassembler.insert( first_index, message.payload, message.FIN, inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  struct TCPReceiverMessage rev_msg;
  // 这里不能用 fin_ 代替 inbound_stream.is_closed()，因为如果 tcp 接收到了 fin，
  // 但是 stream 因为数据不齐全，没有关闭，tcp ack 应该等待数据重新发送，而不是直接确认 fin
  rev_msg.ackno = syn_ ? std::optional<Wrap32> { Wrap32::wrap(
                    inbound_stream.bytes_pushed() + 1 + inbound_stream.is_closed(), isn_ ) }
                       : std::nullopt;
  rev_msg.window_size = std::min( inbound_stream.available_capacity(), static_cast<uint64_t>( UINT16_MAX ) );
  return rev_msg;
}
