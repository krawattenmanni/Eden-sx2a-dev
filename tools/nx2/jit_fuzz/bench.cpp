#include <chrono>
#include <cstdio>
#include <random>
#include "video_core/macro_turing.h"
#include JIT_HEADER
using Word = std::uint32_t;
static void Set(std::array<Word,3>& b, Word low, Word n, Word v){for(Word i=0;i<n;++i) if(v&(1U<<i)) b[(low+i)/32]|=1U<<((low+i)%32);}
int main(){
    // Typical macro shape: r1 = load; method = imm; loop r1 times { emit load0 + r2; r2 += 1 }; end.
    std::vector<Word> code;
    auto put=[&](std::array<Word,3> b){code.push_back(b[2]);code.push_back(b[1]);code.push_back(b[0]);};
    std::array<Word,3> b{};
    // pc0: lane0 ADD r1 = load0 + zero ; out_method0 = imm0 (0x1234)
    Set(b,10,5,0);Set(b,15,5,1);Set(b,20,5,28);Set(b,25,5,24);Set(b,30,16,0x1234);Set(b,82,3,5);put(b);
    // pc1: lane0 LOOP r1, 3 ; lane1 nop
    b={};Set(b,10,5,23);Set(b,15,5,24);Set(b,20,5,1);Set(b,25,5,24);Set(b,30,16,3);Set(b,46,5,0);Set(b,51,5,24);Set(b,56,5,24);Set(b,61,5,24);put(b);
    // pc2: lane0 ADD r2 = r2 + imm(1) ; emit alu0
    b={};Set(b,10,5,0);Set(b,15,5,2);Set(b,20,5,2);Set(b,25,5,25);Set(b,30,16,1);Set(b,85,4,1);put(b);
    // pc3: lane0 XOR r3 = r3 ^ r2 ; emit alu0
    b={};Set(b,10,5,15);Set(b,15,5,3);Set(b,20,5,3);Set(b,25,5,2);Set(b,85,4,1);put(b);
    // pc4: END ; pc5 nop
    b={};Set(b,0,1,1);put(b); b={};put(b);
    std::vector<Word> params{16};
    Word sink=0; const int N=200000;
    auto emit=[&](Word m, Word v){sink+=m^v;return true;};
    auto read=[&](Word){return 0U;};
    Tegra::TuringMacroInterpreter in; Tegra::TuringMacroJit jit; jit.Prepare(code,1);
    std::vector<Word> f;
    auto t0=std::chrono::steady_clock::now();
    for(int i=0;i<N;++i){f=params; auto r=in.Execute(code,0,f,read,emit); if(!r.success){std::printf("interp fail %s\n",std::string(r.error).c_str());return 1;}}
    auto t1=std::chrono::steady_clock::now();
    for(int i=0;i<N;++i){f=params; auto r=jit.Execute(0,f,std::span<Word>{in.dram},read,emit); if(!r.success){std::printf("jit fail %s\n",std::string(r.error).c_str());return 1;}}
    auto t2=std::chrono::steady_clock::now();
    double a=std::chrono::duration<double,std::nano>(t1-t0).count()/N, c=std::chrono::duration<double,std::nano>(t2-t1).count()/N;
    std::printf("macro with 32 emits: interpreter %.0f ns/call, jit %.0f ns/call, %.1fx (sink %u)\n",a,c,a/c,sink);
}
