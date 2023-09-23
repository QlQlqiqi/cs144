#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <queue>

class TCPSender
{
  // 启动定时器
  // 如果 restart 为 true，则定时器将重新启动
  void timer(const bool &restart);
  // 判断定时器是否结束
  bool expired();
  // 删除定时器
  void remove_timer();

  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  // 当前的 RTO
  uint64_t cur_RTO_ms_;
  // 当前时间
  uint64_t cur_time_ms_;
  // 定时器的启动时间
  uint64_t timer_start_ms_;
  // consecutive retransmission 触发次数
  uint64_t con_retrans_cnt_;
  // push msg with next seqno
  Wrap32 next_seqno_;
  // 最大已发送的 msg seqno + len
  Wrap32 max_sent_seqno_;
  // last ack's seqno + 1
  // uint64_t next_ack_idx_;
  // 上一次 ack 携带的 window size
  uint16_t rev_win_size_;
  // 上次接收到的 window size
  // uint16_t last_rev_win_size_;
  // 如果 receiver 返回的 window size 是 0，则记录
  bool rev_win_size_zero_;
  // 是否发送 the earliest msg
  bool need_sent_earliest_msg_;
  // 是否发送了 empty segment with syn
  // bool is_send_syn_;
  // 是否已经收到了携带 syn 的 segment 的 ack
  bool is_syn_;
  // 是否已经发送 fin
  bool is_fin_;
  // 等待 ack 的 segments 的 seq number 总数
  uint64_t out_segs_num_;
  // outstanding segment
  std::queue<TCPSenderMessage> out_segs_;
  // waiting to be sent
  std::queue<TCPSenderMessage> wait_segs_;

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
