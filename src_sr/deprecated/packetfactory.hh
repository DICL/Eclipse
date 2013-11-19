#ifndef __PACKET_FACTORY_HH_
#define __PACKET_FACTORY_HH_

#include <order.hh>
#include <packets.hh>
#include <string.h>

class PacketFactory {
 //friend class Packet, Order, Header;
 public:
  static Packet* fromOrder (Order&);
  static Packet* fromdiskPage (diskPage&);
  static Packet* fromHeader (Header&);
};

#endif
