#include <cstdint>
#include <iostream>
#include <chrono>
#include <cmath>
#include "RingBuffer_upstream.h"
int main(){RingBuffer<float> b(1,1024);float x[128],y[1024];const float* input[1]{x};float* output[1]{y};int count=0;for(int k=0;k<16;k++){for(float& v:x)v=static_cast<float>(count++);b.write(input,0,128);}b.read(output,0,1024);for(int i=0;i<1024;i++)if(y[i] != i+1024){std::cerr<<"wrap mismatch at "<<i<<'\n';return 1;}auto t=std::chrono::steady_clock::now();double checksum=0;for(int k=0;k<100000;k++){b.write(input,0,128);b.read(output,0,1024);checksum+=y[k%1024];}double micros=1e6*std::chrono::duration<double>(std::chrono::steady_clock::now()-t).count()/100000;std::cout<<"wrap verified; 128-write + 1024-read: "<<micros<<" us/iteration; checksum "<<checksum<<'\n';}
