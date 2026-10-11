#include "../src/Engine.hpp"
#include "../src/Presets.hpp"
#include <cstdio>
using namespace kn;
int main(){
  const int sr=48000; const int secs=10;
  for(int i=0;i<kNumPresets;i++){
    static Engine e; e.~Engine(); new(&e) Engine();
    float vals[kTotalParams]={};
    for(int t=0;t<kTracks;t++)for(int p=0;p<kTrackParams;p++)vals[t*kTrackParams+p]=trackParamDefault(t,p);
    buildPreset(i,vals,e.bank);
    // roundtrip bank serialization
    std::string s=serializeBank(e.bank); Bank b2; if(!deserializeBank(b2,s.c_str())||serializeBank(b2)!=s) printf("SERIALIZE MISMATCH %s\n",kPresets[i].name);
    for(int t=0;t<kTracks;t++)for(int p=0;p<kTrackParams;p++)e.trackParam[t][p]=vals[t*kTrackParams+p];
    for(int g=0;g<gOUT_STEP;g++) e.global[g]=vals[kGlobalBase+g];
    e.global[gPLAY]=1; e.init(sr);
    std::vector<float> L(256),R(256); Transport tp; float pk=0; double rms=0; long n=0; int nan=0;
    std::vector<float> all; 
    for(int b=0;b<sr*secs/256;b++){ e.process(L.data(),R.data(),256,tp,nullptr,0); for(int k=0;k<256;k++){ if(!std::isfinite(L[k]))nan++; pk=std::max(pk,std::fabs(L[k])); rms+=L[k]*L[k]+R[k]*R[k]; n+=2; all.push_back(L[k]); all.push_back(R[k]);} }
    unsigned tc=0; for(int t=0;t<kTracks;t++) tc+=e.trigCount[t];
    printf("%-18s peak %.3f rms %.3f trigs %u %s\n",kPresets[i].name,pk,std::sqrt(rms/n),tc,nan?"NAN!":"");
    char fn[96]; snprintf(fn,96,"/tmp/mp_%02d.raw",i); FILE*f=fopen(fn,"wb"); fwrite(all.data(),4,all.size(),f); fclose(f);
  }
}
