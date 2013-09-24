#ifndef __PACKET_FACTORY_HH_
#define __PACKET_FACTORY_HH_

#include <order.hh>
#include <packets.hh>
#include <string.h>

class PacketFactory {
 //friend class Packet, Order, Header;
 public:
  static Packet& fromOrder (Order& in) {
   size_t s;
   Packet* packet = new Packet ();

   packet->data = in.serialize (&s);
   packet->size = s;

   return packet;
  }

  static Packet& fromDiskPage (DiskPage& in) {
   Packet* packet = new Packet ();

   memcpy (packet->data, &(in.chunk), DPSIZE);
   packet->size = DPSIZE;
   packet->set_time (in.time);

   return packet;
  }

  static Packet& fromHeader (Header& in) {
   Packet* packet = new Packet ();
   memcpy (&packet, &in, sizeof (Header));

   return *packet;
  }
};

#endif
