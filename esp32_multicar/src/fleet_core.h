#pragma once
#include <stdint.h>
#include <string.h>
#include <ctype.h>
namespace shellrc {
struct FleetCar {uint8_t address[6]={};uint8_t type=0;char name[48]={};bool seen=false;bool rejected=false;int8_t rssi=0;};
class Fleet {
public:
 static constexpr int capacity=8;
 FleetCar cars[capacity];int count=0;uint8_t preferred[6]={};uint8_t manual[6]={};bool hasManual=false;
 static bool same(const uint8_t*a,const uint8_t*b){return memcmp(a,b,6)==0;}
 static bool prefix(const char*n,const char*p){while(*p){if(toupper((unsigned char)*n++)!=*p++)return false;}return true;}
 int find(const uint8_t*a)const{for(int i=0;i<count;i++)if(same(cars[i].address,a))return i;return -1;}
 void beginScan(){for(int i=0;i<count;i++)cars[i].seen=false;}
 int observe(const uint8_t*a,uint8_t type,const char*name,int8_t rssi){
  int i=find(a);bool candidate=prefix(name,"SL-")||prefix(name,"SHELL")||same(a,preferred)||i>=0;
  if(!candidate)return -1;
  if(i<0){if(count==capacity)return -1;i=count++;memcpy(cars[i].address,a,6);}
  FleetCar&c=cars[i];c.type=type;c.rssi=rssi;c.seen=true;
  if(*name){int k=0;for(;name[k]&&k<47;k++){unsigned char ch=name[k];c.name[k]=(ch>=32&&ch<127&&ch!=','&&ch!='=')?ch:'_';}c.name[k]=0;}
  if(!*c.name)strcpy(c.name,"Shell BLE");
  return i;
 }
 int choose()const{
  if(hasManual){int i=find(manual);return i>=0&&cars[i].seen&&!cars[i].rejected?i:-1;}
  int favorite=find(preferred);if(favorite>=0&&cars[favorite].seen&&!cars[favorite].rejected)return favorite;
  int one=-1;for(int i=0;i<count;i++)if(cars[i].seen&&!cars[i].rejected){if(one>=0)return -1;one=i;}return one;
 }
 bool select(int i){if(i<0||i>=count||cars[i].rejected)return false;memcpy(manual,cars[i].address,6);hasManual=true;return true;}
 int cycle(int direction)const{
  int start=hasManual?find(manual):find(preferred);if(start<0)start=direction>0?-1:0;
  for(int step=1;step<=count;step++){int i=(start+direction*step+count*2)%count;if(!cars[i].rejected)return i;}return -1;
 }
 void remember(const uint8_t*a){memcpy(preferred,a,6);}
};
}
