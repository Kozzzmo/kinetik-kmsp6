#include "../src/Engine.hpp"
#include <cstdio>
#include <vector>
using namespace kn;
static void wav(const char* fn, const std::vector<float>& L, const std::vector<float>& R, int sr){
  FILE* f=fopen(fn,"wb"); int n=L.size(); int ds=n*4; int v;
  fwrite("RIFF",1,4,f); v=36+ds; fwrite(&v,4,1,f); fwrite("WAVEfmt ",1,8,f); v=16; fwrite(&v,4,1,f);
  short s=1; fwrite(&s,2,1,f); s=2; fwrite(&s,2,1,f); fwrite(&sr,4,1,f); v=sr*4; fwrite(&v,4,1,f); s=4; fwrite(&s,2,1,f); s=16; fwrite(&s,2,1,f);
  fwrite("data",1,4,f); fwrite(&ds,4,1,f);
  for(int i=0;i<n;i++){ short a=(short)(std::max(-1.f,std::min(1.f,L[i]))*32767), b=(short)(std::max(-1.f,std::min(1.f,R[i]))*32767); fwrite(&a,2,1,f); fwrite(&b,2,1,f);} fclose(f);}
int main(){
  const int sr=48000;
  for(int m=1;m<kMachines;m++){
    static Engine e; e.~Engine(); new(&e) Engine();
    e.bank.machine[0]=m; for(int p=0;p<8;p++) e.trackParam[0][p]=kMachineDefs[m].def[p];
    for(int t=1;t<kTracks;t++) e.bank.machine[t]=mOFF;
    e.init(sr);
    Pattern& P=e.bank.patterns[0];
    int notes[16]={48,48,60,48,51,48,55,58,48,60,48,63,48,55,46,48};
    for(int s=0;s<16;s++){ if(s%4==3) continue; P.flags[0][s]=sfTrig|(s%5==0?sfAccent:0)|(s==6?sfSlide:0); P.note[0][s]=notes[s]; }
    e.global[gPLAY]=1; e.global[gTEMPO]=125;
    std::vector<float> L(sr*4),R(sr*4); Transport tp;
    for(int i=0;i<sr*4;i+=256) e.process(&L[i],&R[i],256,tp,nullptr,0);
    float pk=0,rms=0; for(int i=0;i<sr*4;i++){pk=std::max(pk,std::fabs(L[i])); rms+=L[i]*L[i];}
    printf("%-6s peak %.3f rms %.3f trigs %u\n",kMachineDefs[m].name,pk,std::sqrt(rms/(sr*4)),e.trigCount[0]);
    char fn[64]; snprintf(fn,64,"/tmp/mono_%s.wav",kMachineDefs[m].name); wav(fn,L,R,sr);
  }
  // arp test
  { static Engine e; e.init(sr); e.bank.kit[0][kfArpMode]=1; e.bank.kit[0][kfArpChord]=5; e.bank.kit[0][kfArpOct]=1;
    Pattern& P=e.bank.patterns[0]; P.flags[0][0]=sfTrig; P.note[0][0]=57; P.gate[0][0]=11;
    for(int t=1;t<kTracks;t++) e.bank.machine[t]=mOFF;
    e.global[gPLAY]=1; std::vector<float> L(sr*4),R(sr*4); Transport tp;
    for(int i=0;i<sr*4;i+=256) e.process(&L[i],&R[i],256,tp,nullptr,0);
    float pk=0; for(float x:L) pk=std::max(pk,std::fabs(x)); printf("arp peak %.3f\n",pk); wav("/tmp/mono_arp.wav",L,R,sr);}
  // live notes + rec
  { static Engine e; e.init(sr); e.recording=true; e.global[gPLAY]=1; Transport tp; std::vector<float> L(256),R(256);
    for(int b=0;b<400;b++){ NoteEvent ev[1]; int n=0; if(b==50){ev[0]={10,2,1.f,64,true};n=1;} if(b==120){ev[0]={10,2,1.f,64,false};n=1;}
      e.process(L.data(),R.data(),256,tp,ev,n);}
    RecEvent r; int c=0; while(e.popRec(r)){c++; printf("rec p%d t%d s%d n%d g%d\n",r.pattern,r.track,r.step,r.note,r.gate);} printf("recs %d\n",c);}
}
