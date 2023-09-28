#include "router.hh"
#include "address.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>

// using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const std::optional<Address> next_hop,
                        const size_t interface_num )
{
  // std::cerr << "============================================\n";
  std::cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
            << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
            << " on interface " << interface_num << "\n";

  table_.emplace_back( TableCell { route_prefix, prefix_length, next_hop, interface_num } );
  is_unordered_ = true;
}

void Router::route()
{
  std::cout << "========================================================" << std::endl;
  std::cout << "route start:" << std::endl;

  // 先进行排序
  if ( is_unordered_ ) {
    std::cout << "route: OrderTable" << std::endl;
    OrderTable();
    is_unordered_ = false;
  }

  for ( auto& inf : interfaces_ ) {
    auto rev = inf.maybe_receive();
    // 如果没有可接收的 dgram，则换下一个
    if ( !rev.has_value() ) {
      continue;
    }

    auto dgram = rev.value();

    // 如果 TTL <= 1，抛弃
    if ( dgram.header.ttl <= 1 ) {
      continue;
    }
    dgram.header.ttl--;
    dgram.header.compute_checksum();

    // 否则将其转发到指定的 interface
    auto dst = dgram.header.dst;
    std::cout << "route: dst " << Address::from_ipv4_numeric( dst ).ip() << std::endl;
    for ( auto& table_cell : table_ ) {
      // 如果匹配，则将其发送到指定的 interface
      if ( Match( dst, table_cell.route_prefix_, table_cell.prefix_length_ ) ) {
        auto& match_inf = interface( table_cell.interface_num_ );
        std::cout << "route: table cell(" << table_cell.interface_num_ << ")"
                  << Address::from_ipv4_numeric( table_cell.route_prefix_ ).ip() << "/"
                  << static_cast<int>( table_cell.prefix_length_ ) << " => "
                  << ( table_cell.next_hop_.has_value() ? table_cell.next_hop_->ip() : "(direct)" ) << std::endl;
        match_inf.send_datagram( dgram, table_cell.next_hop_.value_or( Address::from_ipv4_numeric( dst ) ) );
        break;
      }
    }
  }
}

void Router::OrderTable()
{
  table_.sort( [&]( const TableCell& t1, const TableCell& t2 ) { return t1.prefix_length_ >= t2.prefix_length_; } );
  for ( auto& item : table_ ) {
    std::cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( item.route_prefix_ ).ip() << "/"
              << static_cast<int>( item.prefix_length_ ) << " => "
              << ( item.next_hop_.has_value() ? item.next_hop_->ip() : "(direct)" ) << " on interface "
              << item.interface_num_ << "\n";
  }
}

bool Router::Match( const uint32_t& addr, const uint32_t& route_prefix, const uint8_t& prefix_length )
{
  // std::cout << "match start:" << std::endl;
  // std::cout << Address::from_ipv4_numeric( addr ).ip() << "/" << static_cast<uint32_t>( prefix_length ) << "  "
  //           << Address::from_ipv4_numeric( route_prefix ).ip() << std::endl;
  // std::cout << ( Address::from_ipv4_numeric( static_cast<uint32_t>( addr )
  //                                            >> static_cast<uint32_t>( 32 - prefix_length ) )
  //                  .ip() )
  //           << std::endl;
  return ( static_cast<uint64_t>( route_prefix ) >> static_cast<uint32_t>( 32 - prefix_length ) )
         == ( static_cast<uint64_t>( addr ) >> static_cast<uint32_t>( 32 - prefix_length ) );
}
