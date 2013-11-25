#include <packetfactory.hh>

Packet& PacketFactory::fromOrder (Order& in) {
 size_t s;
 Packet* packet = new Packet ();

 packet->data = in.serialize (&s);
 packet->size = s;

 return packet;
}

Packet& PacketFactory::fromDiskPage (DiskPage& in) {
 Packet* packet = new Packet ();

 memcpy (packet->data, &(in.chunk), DPSIZE);
 packet->size = DPSIZE;
 packet->set_time (in.time);

 return packet;
}

Packet& PacketFactory::fromHeader (Header& in) {
 Packet* packet = new Packet ();
 memcpy (&packet, &in, sizeof (Header));

 return *packet;
}
