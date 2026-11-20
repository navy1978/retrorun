#include "core/run_boundary.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
namespace {
unsigned calls;
void normalRun(){++calls;}
#if defined(__aarch64__) && defined(__GNUC__)
// Deliberately violate AAPCS64 as legacy handwritten core assembly can do.
extern "C" void brokenRun();
#ifdef __APPLE__
#define BROKEN_NAME "_brokenRun"
#else
#define BROKEN_NAME "brokenRun"
#endif
asm(".text\n.global " BROKEN_NAME "\n" BROKEN_NAME ":\n"
    "movi v8.16b, #0\nmovi v9.16b, #0\nmovi v10.16b, #0\nmovi v11.16b, #0\n"
    "movi v12.16b, #0\nmovi v13.16b, #0\nmovi v14.16b, #0\nmovi v15.16b, #0\nret\n");
__attribute__((noinline)) void unprotected(void (*run)()){run();}
__attribute__((noinline)) bool preserved(void (*invoke)(void (*)())){
    const uint64_t before[8]={11,22,33,44,55,66,77,88};uint64_t after[8]{};
    asm volatile("ldp d8,d9,[%0]\nldp d10,d11,[%0,#16]\nldp d12,d13,[%0,#32]\nldp d14,d15,[%0,#48]"
                 ::"r"(before):"v8","v9","v10","v11","v12","v13","v14","v15","memory");
    invoke(brokenRun);
    asm volatile("stp d8,d9,[%0]\nstp d10,d11,[%0,#16]\nstp d12,d13,[%0,#32]\nstp d14,d15,[%0,#48]"
                 ::"r"(after):"memory");
    return std::memcmp(before,after,sizeof before)==0;
}
#endif
}
int main(){
    rr::runCoreFrame(normalRun);assert(calls==1);
#if defined(__aarch64__) && defined(__GNUC__)
    assert(!preserved(unprotected));
    assert(preserved(rr::runCoreFrame));
#endif
    std::cout<<"Core frame callback count and ARM64 SIMD boundary preservation passed\n";
}
