#include "assets.hpp"
#include <iostream>
#include <stdexcept>
using namespace ashford;
void check(bool ok) { if (!ok) throw std::runtime_error("check failed"); }
template<class F> void rejects(F f) { bool failed=false; try { f(); } catch(const std::runtime_error&) { failed=true; } check(failed); }
void put(Bytes& b, size_t at, uint32_t v) { for(int i=0;i<4;++i) b.at(at+i)=uint8_t(v>>(8*i)); }
int main() {
 try {
  check(expand({0x15,'A',0xf9,0xff,0,0}) == Bytes({'A','A','A','A'}));
  check(expand({0x51,'B',0xff,0,0}) == Bytes({'B','B','B','B'}));
  check(expand({0x15,'C',0xf8,0xff,3,0,0}) == Bytes({'C','C','C','C','C'}));
  check(expand({0xff,'A','B','C','D','E','F','G','H',5,'I',0,0}) == Bytes({'A','B','C','D','E','F','G','H','I'}));
  rejects([] { expand({1,'A'}); });
  rejects([] { expand({2,0xf9,0xff}); });
  rejects([] { expand({0x15,'A',0xf9,0xff,0,0},3); });
  rejects([] { expand({2,0,0,99}); });
  check(expand({2,0,0}).empty());
  rejects([] { expand({0,0xff}); });
  rejects([] { expand({2,0xf8,0xff}); });
  rejects([] { expand({1,'A',0,0},0); });
  Bytes afs(32); afs[0]='A';afs[1]='F';afs[2]='S';put(afs,4,2);
  put(afs,8,24);put(afs,12,4);put(afs,16,28);put(afs,20,4);
  check(afs_index(afs,32).size()==2);
  // Sound partitions use "AFS " on the USA disc. CRI adx_fs.c checks
  // the first three signature bytes; the native parser recognizes both forms.
  afs[3]=0x20;check(afs_index(afs,32).size()==2);afs[3]=0;
  rejects([&] { afs_index(afs,31); });
  put(afs,16,26); rejects([&] { afs_index(afs,32); });
  put(afs,16,28);put(afs,8,8); rejects([&] { afs_index(afs,32); });
  put(afs,8,24);afs[3]=1; rejects([&] { afs_index(afs,32); });afs[3]=0;
  put(afs,4,10001);rejects([&] { afs_index(afs,32); });
  Bytes room(1200);put(room,0,0x41200000);put(room,16,128);
  auto info=room_info(room);check(info.version==10.f && info.cameras==0);
  put(room,128+0x80,1); rejects([&] { room_info(room); });
  put(room,128,1132); rejects([&] { room_info(room); });
  room.resize(1812);check(room_info(room).cameras==1);
  put(room,20,1132);put(room,1132,1140);room[1140]='M';room[1141]='D';room[1142]='L';
  room[1144]=16;room[1146]=1;put(room,1152,16);
  check(room_info(room).model_objects==1);
  put(room,1152,0xffffffff);rejects([&] { room_info(room); });
  put(room,0,0x7fc00000);rejects([&] { room_info(room); });
  std::cout << "Asset boundary checks passed\n";
 } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
