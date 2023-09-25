#include "tcp_sender.hh"
#include "buffer.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , cur_RTO_ms_( initial_RTO_ms_ )
  , cur_time_ms_( 0 )
  , timer_start_ms_( static_cast<uint64_t>( -1 ) )
  , con_retrans_cnt_( 0 )
  , next_seqno_( isn_ )
  , max_sent_seqno_( isn_ )
  , rev_win_size_( 1 )
  // , last_rev_win_size_( 1 )
  , rev_win_size_zero_( true )
  , need_sent_earliest_msg_( false )
  , is_syn_( false )
  , is_fin_( false )
  // , can_sent_( false )
  , out_segs_num_( 0 )
  , out_segs_ { {} }
  , wait_segs_ { {} }
{
  // std::cout << "===================================================" << std::endl;
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return out_segs_num_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return con_retrans_cnt_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // std::cout << "maybe_send: start" << std::endl;

  // 发送待发送的 msg
  while ( !wait_segs_.empty() ) {
    auto wait_msg = wait_segs_.front();
    // std::cout << "maybe_send: add wait msg into out segs and send it: wait_msg.seqno " <<
    // wait_msg.seqno.GetValue()
    //           << " wait_msg len " << wait_msg.sequence_length() << std::endl;
    wait_segs_.pop();
    out_segs_.emplace( wait_msg );
    max_sent_seqno_ = wait_msg.seqno + wait_msg.sequence_length();
    return std::optional<TCPSenderMessage> { wait_msg };
  }

  // 定时器到期后，发送 the earliest msg
  if ( need_sent_earliest_msg_ && !out_segs_.empty() ) {
    // std::cout << "maybe_send: timer is expired" << std::endl;
    // if ( out_segs_.empty() ) {
    //   return std::nullopt;
    // }
    need_sent_earliest_msg_ = false;
    // std::cout << "maybe_send: send the earliest msg" << std::endl;
    timer( false );
    return std::optional<TCPSenderMessage> { out_segs_.front() };
  }

  return std::nullopt;
}

void TCPSender::push( Reader& outbound_stream )
{
  // std::cout << "push: start" << std::endl;
  // 如果还未发送 syn，则先只发送 syn
  if ( !is_syn_ && out_segs_num_ == 0 ) {
    TCPSenderMessage msg = { next_seqno_, true, Buffer(), false };
    if ( outbound_stream.is_finished() ) {
      msg.FIN = true;
      is_fin_ = true;
    }
    // std::cout << "push: push empty msg with syn " << ( msg.FIN ? "with fin" : "without fin" )
    //           << ": out_segs_num_: " << out_segs_num_ << " -> " << out_segs_num_ + msg.SYN + msg.FIN <<
    //           std::endl;
    // std::cout << "push 1: out_segs_num_ " << out_segs_num_ << " -> " << out_segs_num_ + msg.SYN + msg.FIN
    //           << std::endl;
    // std::cout << "push 1: next_seqno " << next_seqno_.GetValue() << " -> "
    //           << next_seqno_.GetValue() + msg.SYN + msg.FIN << std::endl;
    next_seqno_ = next_seqno_ + msg.SYN + msg.FIN;
    out_segs_num_ += msg.SYN + msg.FIN;
    wait_segs_.emplace( msg );
    timer( false );
    // wait_segs_.emplace( msg );
    // return std::optional<TCPSenderMessage> { msg };
    return;
  }

  // 如果还未接收到 syn，退出
  if ( !is_syn_ ) {
    return;
  }

  // rev_win_size_ = std::max( rev_win_size_, static_cast<uint16_t>(1) );

  uint16_t max_payload_size = TCPConfig::MAX_PAYLOAD_SIZE;
  std::string str;
  while ( outbound_stream.bytes_buffered() != 0 ) {
    // if ( rev_win_size_zero_ ) {
    //   rev_win_size_ = 1;
    // }
    auto tmp_len = rev_win_size_ <= out_segs_num_
                     ? 0UL
                     : std::min( static_cast<uint64_t>( rev_win_size_ ) - out_segs_num_,
                                 static_cast<uint64_t>( max_payload_size ) );
    if ( rev_win_size_zero_ ) {
      tmp_len = std::max( 1UL, tmp_len );
    }
    // auto seqno = Wrap32::wrap( outbound_stream.bytes_popped() + 1, isn_ );
    read( outbound_stream, tmp_len, str );
    auto len = str.size();
    if ( len == 0 ) {
      // if ( rev_win_size_zero_ ) {
      //   rev_win_size_ = 0;
      // }
      break;
    }
    rev_win_size_zero_ = false;
    TCPSenderMessage msg = { next_seqno_, false, Buffer( str ), false };
    // 可以携带 fin
    if ( outbound_stream.is_finished() && len + 1 + out_segs_num_ <= rev_win_size_ && !is_fin_ ) {
      is_fin_ = true;
      len++;
      msg.FIN = true;
    }
    // rev_win_size_ -= len;
    // std::cout << "push: len " << len << ( msg.FIN ? " with " : " without " ) << "fin"
    //           << " next_seqno_ " << next_seqno_.GetValue() << " -> " << ( next_seqno_ + len ).GetValue()
    //           << " out_segs_num_: " << out_segs_num_ << " -> " << out_segs_num_ + len << std::endl;
    out_segs_num_ += len;
    // std::cout << "push 2: rev_win_size_ " << rev_win_size_ << std::endl;
    // std::cout << "push 2: out_segs_num_ " << out_segs_num_ << " -> " << out_segs_num_ + len << std::endl;
    // std::cout << "push 2: next_seqno " << next_seqno_.GetValue() << " -> " << next_seqno_.GetValue() + len
    //           << std::endl;
    next_seqno_ = next_seqno_ + len;
    wait_segs_.emplace( msg );
    timer( false );
    // out_segs_.emplace( msg );
    if ( msg.FIN ) {
      return;
    }
  }

  // if ( rev_win_size_zero_ ) {
  //   rev_win_size_ = 1;
  // }

  // 发送 fin
  if ( outbound_stream.is_finished() && ( rev_win_size_ > out_segs_num_ || rev_win_size_zero_ ) && !is_fin_ ) {
    rev_win_size_zero_ = false;
    is_fin_ = true;
    // std::cout << "push: send empty msg with fin" << std::endl;
    // auto seqno = Wrap32::wrap( outbound_stream.bytes_popped() + 2, isn_ );
    TCPSenderMessage msg = { next_seqno_, false, Buffer(), true };
    // std::cout << "msg.seqno " << msg.seqno.GetValue() << " out_segs_num_: " << out_segs_num_ << " -> "
    //           << out_segs_num_ + 1 << std::endl;
    // std::cout << "push 3: next_seqno " << next_seqno_.GetValue() << " -> " << next_seqno_.GetValue() + 1
    //           << std::endl;
    next_seqno_ = next_seqno_ + 1;
    out_segs_num_ += 1;
    // out_segs_.emplace( msg );
    wait_segs_.emplace( msg );
    // rev_win_size_--;
    timer( false );
  }

  // if ( rev_win_size_zero_ ) {
  //   rev_win_size_ = 0;
  // }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // std::cout << "send_empty_message: seqno " << next_seqno_.GetValue() << std::endl;
  return { next_seqno_, false, Buffer(), false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // std::cout << "receive: start" << std::endl;
  // 如果 msg.ackno 为空值，则 return
  if ( msg.ackno == std::nullopt ) {
    // std::cout << "receive: msg.ackno is nullopt" << std::endl;
    return;
  }

  // last_rev_win_size_ = msg.window_size;

  auto msg_ackno = msg.ackno.value();
  // std::cout << "msg_ackno: " << msg_ackno.GetValue() << std::endl;

  if ( out_segs_.empty() ) {
    // std::cout << "receive: out_segs_ is empty" << std::endl;
    return;
  }

  auto next_ack_seqno = out_segs_.front().seqno;

  // invalid msg seqno
  if ( next_ack_seqno < max_sent_seqno_ ) {
    if ( msg_ackno < next_ack_seqno || max_sent_seqno_ < msg_ackno ) {
      // std::cout << "receive: get invalid msg seqno" << std::endl;
      return;
    }
  }
  if ( max_sent_seqno_ < next_ack_seqno ) {
    if ( msg_ackno < next_ack_seqno && max_sent_seqno_ < msg_ackno ) {
      // std::cout << "receive: get invalid msg seqno" << std::endl;
      return;
    }
  }

  // std::cout << "receive: window size " << rev_win_size_ << " -> " << msg.window_size << std::endl;
  rev_win_size_ = msg.window_size;
  // std::cout << "isn_: " << isn_.GetValue() << std::endl;
  is_syn_ = is_syn_ || msg_ackno != isn_;

  rev_win_size_zero_ = rev_win_size_ == 0;

  auto old_out_segs_num = out_segs_num_;

  // 1. reset current RTO
  cur_RTO_ms_ = initial_RTO_ms_;

  if ( msg_ackno < next_ack_seqno ) {
    next_ack_seqno = next_ack_seqno + out_segs_.front().sequence_length();
    while ( !out_segs_.empty() ) {
      auto seg = out_segs_.front();
      auto seg_ack_seqno = seg.seqno + seg.sequence_length();
      if ( next_ack_seqno <= seg_ack_seqno || seg_ack_seqno <= msg_ackno ) {
        // std::cout << "out_segs_.pop1" << std::endl;
        // std::cout << "seg_ack_seqno " << seg_ack_seqno.GetValue() << " out_segs_num_: " << out_segs_num_ << " ->
        // "
        //           << out_segs_num_ - seg.sequence_length() << std::endl;
        out_segs_num_ -= seg.sequence_length();
        out_segs_.pop();
        continue;
      }
      if ( seg.seqno < msg_ackno ) {
        // std::cout << "seg.seqno " << seg.seqno.GetValue() << std::endl;
        // std::cout << "receive: window size " << rev_win_size_ << " -> "
        //           << rev_win_size_ - ( seg_ack_seqno.GetValue() - msg_ackno.GetValue() ) << std::endl;
        rev_win_size_ -= ( seg_ack_seqno.GetValue() - msg_ackno.GetValue() );
      }
      break;
    }
  }
  // 否则，next_ack_seqno <= seg.seqno < msg.ackno 即可
  else {
    while ( !out_segs_.empty() ) {
      auto seg = out_segs_.front();
      auto seg_ack_seqno = seg.seqno + seg.sequence_length();
      if ( seg_ack_seqno <= msg_ackno ) {
        // std::cout << "out_segs_.pop2" << std::endl;
        // std::cout << "seg_ack_seqno " << seg_ack_seqno.GetValue() << " out_segs_num_: " << out_segs_num_ << " ->
        // "
        //           << out_segs_num_ - seg.sequence_length() << std::endl;
        out_segs_num_ -= seg.sequence_length();
        out_segs_.pop();
        continue;
      }
      if ( seg.seqno < msg_ackno ) {
        // std::cout << "seg.seqno " << seg.seqno.GetValue() << std::endl;
        // std::cout << "receive: window size " << rev_win_size_ << " -> "
        //           << rev_win_size_ - ( seg_ack_seqno.GetValue() - msg_ackno.GetValue() ) << std::endl;
        rev_win_size_ -= ( seg_ack_seqno.GetValue() - msg_ackno.GetValue() );
      }
      break;
    }
  }

  // 3. 重启定时器
  if ( out_segs_num_ == 0 ) {
    remove_timer();
  } else if ( old_out_segs_num != out_segs_num_ ) {
    timer( true );
  }

  // 4. 设置 consective retransmission 为 0
  con_retrans_cnt_ = 0;
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // std::cout << "tick: start" << std::endl;
  // std::cout << "tick: current time " << cur_time_ms_ << " -> " << cur_time_ms_ + ms_since_last_tick
  //           << " current RTO " << cur_RTO_ms_ << " ms_since_last_tick " << ms_since_last_tick << "
  //           timer_start_ms_ "
  //           << timer_start_ms_ << " window size " << rev_win_size_ << std::endl;
  cur_time_ms_ += ms_since_last_tick;
  if ( !expired() ) {
    return;
  }
  // std::cout << "tick: timer is expired" << std::endl;
  need_sent_earliest_msg_ = true;
  if ( rev_win_size_ != 0 ) {
    // std::cout << "con_retrans_cnt_ " << con_retrans_cnt_ << " -> " << con_retrans_cnt_ + 1 << std::endl;
    con_retrans_cnt_++;
    // std::cout << "cur_RTO_ms_ " << cur_RTO_ms_ << " -> " << ( cur_RTO_ms_ << 1 ) << std::endl;
    cur_RTO_ms_ <<= 1;
  }
  timer( true );
}

void TCPSender::timer( const bool& restart )
{
  if ( restart || timer_start_ms_ == static_cast<uint64_t>( -1 ) ) {
    timer_start_ms_ = cur_time_ms_;
    // std::cout << "timer: set timer_start_ms_ " << timer_start_ms_ << std::endl;
    return;
  }
}

bool TCPSender::expired()
{
  return cur_time_ms_ - timer_start_ms_ >= cur_RTO_ms_ && cur_time_ms_ >= timer_start_ms_;
}

void TCPSender::remove_timer()
{
  // std::cout << "remove_timer: remove timer" << std::endl;
  timer_start_ms_ = static_cast<uint64_t>( -1 );
}