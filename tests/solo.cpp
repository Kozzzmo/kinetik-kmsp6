#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
using namespace kn;
int main(int argc,char**argv){
  const int sr=48000; const int secs=8;
  for(int i=1;i<kNumPresets;i++){
    printf("%-18s",kPresets[i].name);
    for(int solo=0;solo<kTracks;solo++){
    static Engine e; e.~Engine(); new(&e) Engine();
    float vals[kTotalParams]={};
    buildPreset(i,vals,e.bank);
    for(int t=0;t<kTracks;t++){ e.bank.mute[t]= t!=solo; for(int p=0;p<kTrackParams;p++)e.trackParam[t][p]=vals[t*kTrackParams+p];}
    for(int g=0;g<gOUT_STEP;g++) e.global[g]=vals[kGlobalBase+g];
    e.global[gECHO_LEV]=0; e.global[gREV_LEV]=0;
    e.global[gPLAY]=1; e.init(sr);
    std::vector<float> L(256),R(256); Transport tp; float pk=0; double rms=0; long n=0;
    for(int b=0;b<sr*secs/256;b++){ e.process(L.data(),R.data(),256,tp,nullptr,0); for(int k=0;k<256;k++){ pk=std::max(pk,std::fabs(L[k])); rms+=L[k]*L[k]+R[k]*R[k]; n+=2;} }
    printf("  %s %.2f/%.3f",kMachineDefs[e.bank.machine[solo]].name,pk,std::sqrt(rms/n));
    }
    printf("\n");
  }
}
