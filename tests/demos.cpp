#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
using namespace kn;
static void wav(const char* fn, const std::vector<float>& L, const std::vector<float>& R, int sr){
  FILE* f=fopen(fn,"wb"); int n=L.size(); int ds=n*4; int v;
  fwrite("RIFF",1,4,f); v=36+ds; fwrite(&v,4,1,f); fwrite("WAVEfmt ",1,8,f); v=16; fwrite(&v,4,1,f);
  short s=1; fwrite(&s,2,1,f); s=2; fwrite(&s,2,1,f); fwrite(&sr,4,1,f); v=sr*4; fwrite(&v,4,1,f); s=4; fwrite(&s,2,1,f); s=16; fwrite(&s,2,1,f);
  fwrite("data",1,4,f); fwrite(&ds,4,1,f);
  for(int i=0;i<n;i++){ short a=(short)(std::max(-1.f,std::min(1.f,L[i]))*32767), b=(short)(std::max(-1.f,std::min(1.f,R[i]))*32767); fwrite(&a,2,1,f); fwrite(&b,2,1,f);} fclose(f);}
int main(int argc,char**argv){
  const int sr=44100;
  for(int i=1;i<kNumPresets;i++){
    static Engine e; e.~Engine(); new(&e) Engine();
    float vals[kTotalParams]={}; buildPreset(i,vals,e.bank);
    for(int t=0;t<kTracks;t++)for(int p=0;p<kTrackParams;p++)e.trackParam[t][p]=vals[t*kTrackParams+p];
    for(int g=0;g<gOUT_STEP;g++) e.global[g]=vals[kGlobalBase+g];
    e.init(sr);
    // play pattern A01 twice, then A02 twice if it exists
    bool has2=false; for(int t=0;t<kTracks;t++)for(int s=0;s<kSteps;s++) has2|=e.bank.patterns[1].flags[t][s]!=0;
    const double bpm=vals[kGlobalBase+gTEMPO]; const int len1=e.bank.patterns[0].length, len2=e.bank.patterns[1].length;
    const double steps = 2*len1 + (has2?2*len2:0) + (len1<=16? 2*len1:0);
    const long playN=(long)(steps*0.25*60.0/bpm*sr), total=playN+sr*3;
    std::vector<float> L(total),R(total); Transport tp; e.global[gPLAY]=1;
    long pos=0; bool sw=false;
    while(pos<total){ int n=(int)std::min<long>(256,total-pos);
      if(pos>=playN) e.global[gPLAY]=0;
      if(has2 && !sw && pos > (long)((2*len1-4)*0.25*60.0/bpm*sr)){ e.global[gPATTERN]=1; sw=true; }
      e.process(&L[pos],&R[pos],n,tp,nullptr,0); pos+=n; }
    char fn[128], nm[64]; snprintf(nm,64,"%s",kPresets[i].name); for(char*c=nm;*c;c++) if(*c==' ')*c='-';
    snprintf(fn,128,"%s/%02d-%s.wav",argv[1],i,nm); wav(fn,L,R,sr); printf("%s %.1fs\n",fn,(double)total/sr);
  }
}
