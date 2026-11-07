#include "transport.hpp"
#include <cassert>
#include <map>
#include <vector>
#include <iostream>
using namespace ps_trial;
struct Mock: RegisterIo {
 std::map<uint32_t,uint32_t> words;
 std::vector<std::pair<uint32_t,uint32_t>> writes;
 bool flip=false;unsigned token_reads=0;
 uint32_t read(uint32_t a)override{if(a==16&&flip&&++token_reads==2)return words[a]+1;return words[a];}
 void write(uint32_t a,uint32_t v)override{writes.push_back({a,v});}
};
int main(){Mock m;m.words[0]=0x50534531;Transport t(m);assert(m.writes.empty());
 m.words[16]=7;m.words[20]=100000;m.words[36]=12;
 m.words[128]=m.words[132]=UINT32_MAX;
 for(unsigned i=0;i<4;++i){m.words[136+i*32]=100+i;m.words[144+i*32]=99999;}
 auto s=t.capture();assert(s.token==7&&s.tick==100000&&s.enc[3].negative&&s.enc[3].count==103&&s.position[0]==-1);
 Frame f;f.session=2;f.sequence=3;f.token=7;f.duty={{65536,32768,-32768,-65536}};
 m.words[28]=3;t.publish(f);assert(m.writes.size()==9);
 for(unsigned i=0;i<4;++i)assert(m.writes[4+i]==std::make_pair(76+4*i,uint32_t(f.duty[i])));
 assert(m.writes.back()==std::make_pair(92U,1U));m.words[32]=2;assert(t.applied()==2);
 bool torn=false;m.flip=true;try{t.capture();}catch(const std::runtime_error&){torn=true;}assert(torn);
 bool rejected=false;m.words[0]=0x444d4932;
 try{Transport wrong(m);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 std::cout<<"PASS: experimental identity guard, snapshot decoding, signed four-duty packing, acceptance distinct from application, no implicit ARM\n";
}
