#include <cassert>
#include <vector>
#include "../vendor/USB_Host_Shield_2.0/XboxPollingPolicy.h"
using xbox360_polling::StatusScheduler;
using xbox360_polling::pollInputEndpoint;
int main(){
 StatusScheduler u;
 std::vector<unsigned> probes;
 for(unsigned t=1;t<5001;t+=5) {
   auto slot=u.next(t,StatusScheduler::none);
   if(slot!=StatusScheduler::none)probes.push_back(slot);
 }
 assert(probes.size()>=19 && probes.size()<=21);
 for(unsigned i=0;i<probes.size();i++)assert(probes[i]==i%4);
 StatusScheduler paired;
 unsigned lastStatus=0,count=0;
 for(unsigned t=1;t<30001;t+=4){
   auto slot=paired.next(t,2);
   if(slot!=StatusScheduler::none){
     assert(slot==2);
     if(count)assert(t-lastStatus>=1000 && t-lastStatus<=1004);
     lastStatus=t; ++count;
   }
 }
 assert(count==30);
 assert(pollInputEndpoint(2,2));
 for(int i=0;i<4;i++)if(i!=2)assert(!pollInputEndpoint(i,2));
 for(int i=0;i<4;i++)assert(pollInputEndpoint(i,StatusScheduler::none));
 // Virtual normalized USB operation model for 30-second paired driving,
 // 250 polls/s, one controller. Unlike measured MCU latency, these are
 // deliberately fixed assumed costs to compare algorithmic operations.
 const int polls=30*250, oldInputTransfers=4*polls, newInputTransfers=polls;
 const int oldOutQueries=4*30, newOutQueries=30;
 assert(oldInputTransfers/newInputTransfers==4);
 assert(oldOutQueries/newOutQueries==4);
 return 0;
}
