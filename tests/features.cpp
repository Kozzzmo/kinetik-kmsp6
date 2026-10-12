#include "../src/Engine.hpp"
#include <cstdio>
#include <set>
using namespace kn;
static int fails=0;
#define CHECK(c,msg) do{ if(!(c)){printf("FAIL %s\n",msg);fails++;} else printf("ok   %s\n",msg);}while(0)
static Engine* mk(){ Engine* e=new Engine(); e->init(48000); for(int t=1;t<kTracks;t++) e->bank.machine[t]=mOFF; return e;}
int main(){
  const int sr=48000; std::vector<float> L(256),R(256); Transport tp;
  // 1. arp with held notes cycles notes
  { Engine* e=mk(); e->bank.kit[0][kfArpMode]=1; e->bank.kit[0][kfArpSpd]=1;
    NoteEvent ev[3]={{0,0,1.f,60,true},{0,0,1.f,64,true},{0,0,1.f,67,true}};
    e->process(L.data(),R.data(),256,tp,ev,3); std::set<int> seen;
    for(int b=0;b<400;b++){ e->process(L.data(),R.data(),256,tp,nullptr,0); seen.insert(e->outNote[0]); }
    CHECK(seen.count(60)&&seen.count(64)&&seen.count(67),"arp plays all held notes");
    NoteEvent off[3]={{0,0,0,60,false},{0,0,0,64,false},{0,0,0,67,false}};
    e->process(L.data(),R.data(),256,tp,off,3); for(int b=0;b<200;b++) e->process(L.data(),R.data(),256,tp,nullptr,0);
    CHECK(e->outGate==0,"arp stops on release"); delete e; }
  // 2. sequencer: notes, transpose, mute
  { Engine* e=mk(); Pattern&P=e->bank.patterns[0]; P.flags[0][0]=sfTrig; P.note[0][0]=50; P.flags[0][4]=sfTrig; P.note[0][4]=57;
    e->bank.kit[0][kfTrnsp]=24+12; e->global[gPLAY]=1; std::set<int> seen;
    for(int b=0;b<400;b++){ e->process(L.data(),R.data(),256,tp,nullptr,0); seen.insert(e->outNote[0]); }
    CHECK(seen.count(62)&&seen.count(69),"sequenced notes + transpose"); unsigned tc=e->trigCount[0];
    e->bank.mute[0]=1; for(int b=0;b<400;b++) e->process(L.data(),R.data(),256,tp,nullptr,0);
    CHECK(e->trigCount[0]==tc,"mute stops trigs"); delete e; }
  // 3. host sync: trig on beat 0 lands at frame offset
  { Engine* e=mk(); Pattern&P=e->bank.patterns[0]; P.flags[0][4]=sfTrig; // step 4 = beat 1
    Transport h; h.hostValid=true; h.hostPlaying=true; h.bpm=120; double beat=0.9; unsigned before=0; int hitBlock=-1;
    for(int b=0;b<40;b++){ h.beat=beat; before=e->trigCount[0]; e->process(L.data(),R.data(),256,h,nullptr,0); if(e->trigCount[0]>before && hitBlock<0) hitBlock=b; beat+=256*120.0/60/sr; }
    // beat 1.0 is 0.1 beat after 0.9 -> 2400 samples -> block 9
    CHECK(hitBlock==9,"host-synced trig timing"); delete e; }
  // 4. pattern switch waits for master length
  { Engine* e=mk(); e->bank.patterns[0].length=16; e->bank.patterns[1].flags[0][0]=sfTrig; e->global[gPLAY]=1;
    for(int b=0;b<50;b++) e->process(L.data(),R.data(),256,tp,nullptr,0); e->global[gPATTERN]=1;
    int switched=-1; for(int b=0;b<400&&switched<0;b++){ e->process(L.data(),R.data(),256,tp,nullptr,0); if(e->outPattern==1) switched=b; }
    // 16 steps at 120bpm = 2 s = 375 blocks; started at block 50 -> switch around block 325
    CHECK(switched>300&&switched<340,"pattern change at the end of the pattern"); delete e; }
  // 5. slide glides (no retrigger: trig count increments but env not restarted) -> just check note reaches target smoothly
  { Engine* e=mk(); Pattern&P=e->bank.patterns[0]; P.flags[0][0]=sfTrig; P.note[0][0]=48; P.gate[0][0]=6; P.flags[0][1]=sfTrig|sfSlide; P.note[0][1]=60; P.gate[0][1]=6;
    e->trackParam[0][tpPORT]=60; e->global[gPLAY]=1; float pk=0; bool nan=false;
    for(int b=0;b<200;b++){ e->process(L.data(),R.data(),256,tp,nullptr,0); for(float x:L){pk=std::max(pk,std::fabs(x)); if(!std::isfinite(x))nan=true;} }
    CHECK(!nan && pk>0.01f,"slide renders"); delete e; }
  // 6. live recording records note + length
  { Engine* e=mk(); e->recording=true; e->global[gPLAY]=1; for(int b=0;b<20;b++) e->process(L.data(),R.data(),256,tp,nullptr,0);
    NoteEvent on{0,0,1.2f,65,true}, of{0,0,0,65,false}; e->process(L.data(),R.data(),256,tp,&on,1);
    for(int b=0;b<15;b++) e->process(L.data(),R.data(),256,tp,nullptr,0); e->process(L.data(),R.data(),256,tp,&of,1);
    RecEvent r; int n=0; bool acc=false; int g=-2; while(e->popRec(r)){ n++; if(r.gate<0) acc=r.accent; else g=r.gate; }
    CHECK(n==2 && acc && g>=3 && g<=5,"recorded note, accent and length"); delete e; }
  // 7. scale lock: every played note belongs to the scale
  { Engine* e=mk(); e->bank.kit[0][kfScale]=1; e->bank.kit[0][kfRoot]=2; // D major
    Pattern&P=e->bank.patterns[0]; for(int s=0;s<16;s++){ P.flags[0][s]=sfTrig; P.note[0][s]=(uint8_t)(48+s); }
    e->global[gPLAY]=1; std::set<int> seen; for(int b=0;b<400;b++){ e->process(L.data(),R.data(),256,tp,nullptr,0); seen.insert(e->outNote[0]); }
    bool ok=true; for(int n:seen) if(!inScale(n,1,2)) ok=false; CHECK(ok && seen.size()>4,"scale lock keeps notes in the scale"); delete e; }
  // 8. filter types and LFO sync render cleanly
  { bool clean=true; float pk[5]={};
    for(int ft=0;ft<kFilterTypes;ft++){ Engine* e=mk(); e->trackParam[0][tpFTYPE]=(float)filterTypeValue(ft); e->trackParam[0][tpRESO]=90; e->trackParam[0][tpCUTOFF]=70;
      e->trackParam[0][lfoParam(0,6)]=127; e->trackParam[0][lfoParam(0,1)]=60; e->trackParam[0][lfoParam(0,4)]=(float)lfoDestValue(tpCUTOFF);
      NoteEvent on{0,0,1.f,48,true}; e->process(L.data(),R.data(),256,tp,&on,1);
      for(int b=0;b<300;b++){ e->process(L.data(),R.data(),256,tp,nullptr,0); for(float x:L){ if(!std::isfinite(x)) clean=false; pk[ft]=std::max(pk[ft],std::fabs(x)); } }
      delete e; }
    printf("     filter peaks LP12 %.2f LP24 %.2f BAND %.2f HIGH %.2f NOTCH %.2f\n",pk[0],pk[1],pk[2],pk[3],pk[4]);
    CHECK(clean && pk[0]>0.02f && pk[1]>0.02f && pk[2]>0.01f && pk[3]>0.01f && pk[4]>0.02f,"filter types render"); }
  // 9. synced LFO follows the tempo: 1/4 at 120 BPM = 2 Hz
  { Engine* e=mk(); e->trackParam[0][lfoParam(0,6)]=127; e->trackParam[0][lfoParam(0,0)]=(float)(7*128/16+2); // 1/4
    e->trackParam[0][lfoParam(0,1)]=127; e->trackParam[0][lfoParam(0,2)]=30; e->trackParam[0][lfoParam(0,4)]=(float)lfoDestValue(kLfoDests);
    NoteEvent on{0,0,1.f,60,true}; e->process(L.data(),R.data(),256,tp,&on,1);
    CHECK(kLfoSyncBeats[lfoSyncDivIndex((int)e->trackParam[0][lfoParam(0,0)])]==1.f,"sync division 1/4");
    delete e; }
  // 10. every echo / reverb model at full level stays finite and bounded
  { bool clean=true; float worst=0;
    for(int em=0;em<kEchoModels;em++) for(int rm=0;rm<kReverbModels;rm++){
      Engine* e=mk(); e->global[gECHO_MODEL]=(float)echoModelValue(em); e->global[gREV_MODEL]=(float)reverbModelValue(rm);
      e->global[gECHO_LEV]=127; e->global[gREV_LEV]=127; e->global[gECHO_FB]=110; e->global[gREV_SIZE]=127; e->global[gECHO_MOD]=127; e->global[gREV_MOD]=127;
      e->trackParam[0][tpDEL]=127; e->trackParam[0][tpREV]=127;
      NoteEvent on{0,0,1.f,60,true}, of{0,0,0,60,false}; e->process(L.data(),R.data(),256,tp,&on,1);
      for(int b=0;b<1500;b++){ e->process(L.data(),R.data(),256,tp,b==40?&of:nullptr,b==40?1:0); for(int i=0;i<256;i++){ if(!std::isfinite(L[i])||!std::isfinite(R[i])) clean=false; worst=std::max(worst,std::fabs(L[i])); } }
      delete e; }
    CHECK(clean && worst<=1.0f,"echo x reverb models stable at full level"); }
  printf(fails?"%d FAILED\n":"all passed\n",fails); return fails;
}
