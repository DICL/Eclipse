#include <packets.hh>

Query::Query (const Packet& p): Packet(p) { }
Query::Query (const Query& that): Packet(that) {
  scheduledDate = that.scheduledDate;
  startDate = that.startDate;
  finishedDate = that.finishedDate;
}

void Query::setScheduledDate () {
  gettimeofday (&scheduledDate, NULL);

}
void Query::setStartDate() {
  gettimeofday (&startDate, NULL);
}

void Query::setFinishedDate() {
  gettimeofday (&finishedDate, NULL);
}

uint64_t Query::getWaitTime() {
  return timediff (&startDate, &scheduledDate);
}

uint64_t Query::getExecTime() {
  return timediff (&finishedDate, &startDate);
}
