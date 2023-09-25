#pragma once

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <queue>
#include <unordered_map>
#include <utility>

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface
{
private:
  // Ethernet (known as hardware, network-access, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as Internet-layer or network-layer) address of the interface
  Address ip_address_;

  // 给相同 addr 发送 ARP 的间隔时间
  static constexpr size_t ARP_GAP_MS_ { 5 * 1000UL };
  // addr map 保存的时间长度
  static constexpr size_t ARP_MAP_MS_ { 30 * 1000UL };

  // current time ms
  size_t cur_time_ms_;
  // Address -> (过期时间, EthernetAddress)
  std::map<uint32_t, std::pair<size_t, EthernetAddress>> hop_addr2eth_addr_;
  // 待发送的 EthernetFrame
  std::queue<EthernetFrame> wait_eth_frame_ {};
  // 待发送的 ARP msg
  std::queue<std::pair<EthernetHeader, ARPMessage>> wait_arp_msg_ {};
  // Address -> (等待的过期时间, 等待 ARP 广播的 reply 的 InternetDatagram)
  std::map<uint32_t, std::pair<size_t, std::queue<InternetDatagram>>> hop_addr2arp_expired_ms_;

public:
  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address );

  // Access queue of Ethernet frames awaiting transmission
  std::optional<EthernetFrame> maybe_send();

  // Sends an IPv4 datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address
  // for the next hop.
  // ("Sending" is accomplished by making sure maybe_send() will release the frame when next called,
  // but please consider the frame sent as soon as it is generated.)
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, returns the datagram.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  std::optional<InternetDatagram> recv_frame( const EthernetFrame& frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

  std::string summary( const EthernetFrame& frame );
};
