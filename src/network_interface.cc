#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"
#include <algorithm>
#include <optional>
#include <queue>
#include <utility>

// using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  , cur_time_ms_( 0 )
  , hop_addr2eth_addr_( {} )
  , hop_addr2arp_expired_ms_( {} )
{
  // std::cout << "==============================================" << std::endl;
  // std::cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ )
  //           << " and IP address " << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // std::cout << "[send_datagram: start] next_hop " << next_hop.to_string() << std::endl;

  auto next_hop_addr_ip = next_hop.ipv4_numeric();
  auto eth_addr_it = hop_addr2eth_addr_.find( next_hop_addr_ip );
  auto eth_header = EthernetHeader();
  eth_header.src = ethernet_address_;

  // 如果 next hop addr 已知,则将其加入到待发送的 eth frame 中
  if ( eth_addr_it != hop_addr2eth_addr_.end() ) {
    eth_header.dst = eth_addr_it->second.second;
    eth_header.type = EthernetHeader::TYPE_IPv4;
    auto eth_frame = EthernetFrame { eth_header, serialize<InternetDatagram>( dgram ) };
    wait_eth_frame_.emplace( eth_frame );
    // std::cout << "send_datagram: push into wait_eth_frame_ with " << summary( eth_frame ) << std::endl;
    return;
  }

  // 如果未知,则添加一个 ARP 广播,并记录过期时间和需要发送的 ipv4 数据包q
  // 如果过去一段时间内已经发送了这个 addr 的 ARP 广播,则不发送
  auto expired_it = hop_addr2arp_expired_ms_.find( next_hop_addr_ip );
  if ( expired_it != hop_addr2arp_expired_ms_.end() ) {
    expired_it->second.second.emplace( dgram );
    // std::cout << "send_datagram: there are ARP sent" << std::endl;
    return;
  }

  // std::cout << "send_datagram: dgram " << dgram.header.to_string() << std::endl;

  auto dgram_queue = std::queue<InternetDatagram>();
  dgram_queue.emplace( dgram );
  hop_addr2arp_expired_ms_[next_hop_addr_ip]
    = std::make_pair( cur_time_ms_ + NetworkInterface::ARP_GAP_MS_, dgram_queue );

  eth_header.dst = ETHERNET_BROADCAST;
  eth_header.type = EthernetHeader::TYPE_ARP;
  auto arp_msg = ARPMessage();
  arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
  arp_msg.sender_ethernet_address = ethernet_address_;
  arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
  // arp_msg.target_ethernet_address = ETHERNET_BROADCAST;
  arp_msg.target_ip_address = next_hop_addr_ip;

  wait_arp_msg_.emplace( eth_header, arp_msg );

  auto eth_frame = EthernetFrame { eth_header, serialize<ARPMessage>( arp_msg ) };
  // std::cout << "send_datagram: push into wait_arp_msg_ with " << summary( eth_frame ) << std::endl;
}

// frame: the incoming Ethernet frame
std::optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // std::cout << "[recv_frame: start]" << std::endl;
  // std::cout << "recv_frame: rev frame " << summary( frame ) << std::endl;

  auto eth_header = frame.header;

  if ( eth_header.type != EthernetHeader::TYPE_ARP && eth_header.type != EthernetHeader::TYPE_IPv4 ) {
    // std::cout << "recv_frame: invalid eth header type" << std::endl;
    return std::nullopt;
  }

  // 如果接收到的是 ipv4 数据包
  if ( eth_header.type == EthernetHeader::TYPE_IPv4 && eth_header.dst == ethernet_address_ ) {
    auto data_gram = InternetDatagram();
    if ( !parse<InternetDatagram>( data_gram, frame.payload ) ) {
      // std::cout << "recv_frame: InternetDatagram parse failed" << std::endl;
      return std::nullopt;
    }
    // std::cout << "recv_frame: InternetDatagram parse successfully" << std::endl;
    return std::optional<InternetDatagram> { data_gram };
  }

  // 接收到的是 ARP
  auto arp_msg = ARPMessage();
  if ( !parse<ARPMessage>( arp_msg, frame.payload ) ) {
    // std::cout << "recv_frame: ARPMessage parse failed" << std::endl;
    return std::nullopt;
  }

  // 记录 sender addr -> sender eth addr 的映射，
  hop_addr2eth_addr_[arp_msg.sender_ip_address]
    = std::make_pair( cur_time_ms_ + NetworkInterface::ARP_MAP_MS_, arp_msg.sender_ethernet_address );

  // 如果是广播 eth frame
  if ( eth_header.dst == ETHERNET_BROADCAST ) {
    // 当 addr 不是 target ip addr 时舍弃
    if ( ip_address_.ipv4_numeric() != arp_msg.target_ip_address ) {
      // std::cout << "recv_frame: it is a broadcast but not equal to target ip addr" << std::endl;
      return std::nullopt;
    }

    // 返回一个 reply
    auto reply_arp_msg = ARPMessage();
    reply_arp_msg.opcode = ARPMessage::OPCODE_REPLY;
    reply_arp_msg.sender_ethernet_address = ethernet_address_;
    reply_arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
    reply_arp_msg.target_ethernet_address = arp_msg.sender_ethernet_address;
    reply_arp_msg.target_ip_address = arp_msg.sender_ip_address;

    auto reply_eth_header
      = EthernetHeader { arp_msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP };
    auto eth_frame = EthernetFrame { reply_eth_header, serialize<ARPMessage>( reply_arp_msg ) };
    wait_eth_frame_.emplace( eth_frame );
    // std::cout << "recv_frame: push into wait_eth_frame_ with " << summary( eth_frame ) << std::endl;
    return std::nullopt;
  }

  // 如果是 ARP reply，则将等待 arp reply 的 ipv4 gram 移至待发送的 Ethernetrame
  // hop_addr2eth_addr_[arp_msg.sender_ip_address]
  //   = std::make_pair( cur_time_ms_ + NetworkInterface::ARP_MAP_MS_, arp_msg.sender_ethernet_address );

  // 如果是 request,且保存了 target ip addr 到其 eth addr 的映射,则返回 reply
  // if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST ) {
  //   auto eth_it = hop_addr2eth_addr_.find( arp_msg.target_ip_address );
  //   if ( eth_it == hop_addr2eth_addr_.end() ) {
  //     std::cout << "recv_frame: none map from target ip addr" << std::endl;
  //     return std::nullopt;
  //   }
  //   arp_msg.target_ethernet_address = eth_it->second.second;
  //   // arp_msg.target_ip_address = ip_address_;
  //   auto reply_eth_header
  //     = EthernetHeader { arp_msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP };
  //   auto eth_frame = EthernetFrame { reply_eth_header, serialize<ARPMessage>( arp_msg ) };
  //   wait_eth_frame_.emplace( eth_frame );
  //   std::cout << "recv_frame: push into wait_eth_frame_ with " << summary( eth_frame ) << std::endl;
  //   return std::nullopt;
  // }

  // 如果是 ARP reply，则将等待 arp reply 的 ipv4 gram 移至待发送的 Ethernetrame
  auto arp_it = hop_addr2arp_expired_ms_.find( arp_msg.sender_ip_address );
  if ( arp_it == hop_addr2arp_expired_ms_.end() ) {
    // std::cout << "recv_frame: none InternetDatagram that will be sent" << std::endl;
    return std::nullopt;
  }
  auto arp_queue = arp_it->second.second;
  while ( !arp_queue.empty() ) {
    auto arp_front = arp_queue.front();
    arp_queue.pop();

    auto reply_eth_header
      = EthernetHeader { arp_msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_IPv4 };
    auto eth_frame = EthernetFrame { reply_eth_header, serialize<InternetDatagram>( arp_front ) };
    // std::cout << "recv_frame: push into wait_eth_frame_ with " << summary( eth_frame ) << std::endl;
    wait_eth_frame_.emplace( eth_frame );
  }
  hop_addr2arp_expired_ms_.erase( arp_it );
  // std::cout << "recv_frame: remove all InternetDatagram that will be sent" << std::endl;
  return std::nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // std::cout << "[tick: start] " << ms_since_last_tick << std::endl;

  cur_time_ms_ += ms_since_last_tick;

  // 移除过期的 addr
  for ( auto it = hop_addr2arp_expired_ms_.begin(); it != hop_addr2arp_expired_ms_.end(); ) {
    if ( it->second.first <= cur_time_ms_ ) {
      // std::cout << "tick: remove all InternetDatagram in addr " << it->first << std::endl;
      it = hop_addr2arp_expired_ms_.erase( it );
      continue;
    }
    it++;
  }

  // 移除过期的 map
  for ( auto it = hop_addr2eth_addr_.begin(); it != hop_addr2eth_addr_.end(); ) {
    if ( it->second.first <= cur_time_ms_ ) {
      // std::cout << "tick: remove map " << it->second.second << "/" <<it->first  << std::endl;
      it = hop_addr2eth_addr_.erase( it );
      continue;
    }
    it++;
  }
}

std::optional<EthernetFrame> NetworkInterface::maybe_send()
{
  // std::cout << "[maybe_send: start]" << std::endl;

  // 待发送的 eth frame
  if ( !wait_eth_frame_.empty() ) {
    auto eth_frame = wait_eth_frame_.front();
    wait_eth_frame_.pop();
    // std::cout << "maybe_send: send eth frame " << summary( eth_frame ) << std::endl;
    return std::optional<EthernetFrame> { eth_frame };
  }

  // 待发送的 ARP
  if ( !wait_arp_msg_.empty() ) {
    auto item = wait_arp_msg_.front();
    wait_arp_msg_.pop();
    auto header = item.first;
    auto arp_msg = item.second;
    auto eth_frame = EthernetFrame { header, serialize<ARPMessage>( arp_msg ) };
    // std::cout << "maybe_send: send arp frame " << summary( eth_frame ) << std::endl;
    return std::optional<EthernetFrame> { eth_frame };
  }

  return std::nullopt;
}

std::string NetworkInterface::summary( const EthernetFrame& frame )
{
  std::string out = frame.header.to_string() + ", payload: ";
  switch ( frame.header.type ) {
    case EthernetHeader::TYPE_IPv4: {
      InternetDatagram dgram;
      if ( parse( dgram, frame.payload ) ) {
        out.append( "IPv4: " + dgram.header.to_string() );
        // out.append( " payload:" );
        // for ( auto& item : dgram.payload ) {
        //   out.append( item.release() );
        // }
      } else {
        out.append( "bad IPv4 datagram" );
      }
    } break;
    case EthernetHeader::TYPE_ARP: {
      ARPMessage arp;
      if ( parse( arp, frame.payload ) ) {
        out.append( "ARP: " + arp.to_string() );
      } else {
        out.append( "bad ARP message" );
      }
    } break;
    default:
      out.append( "unknown frame type" );
      break;
  }
  return out;
}