// Separate quiet-run benchmark: do not run concurrently with sanitizer/render jobs.
#define main probe_program_main
#include "fx_probe.cpp"
#undef main
int main(){
 std::cout<<"candidate,run,sample_rate,block,frames,cpu_percent_one_core,worst_block_us,ordinary_new_allocations\n";
 for(int run=1;run<=2;++run)for(std::string id:{"airwindows","dspark","patina","daisy","0xfx"}){
  auto a=create(id,48000);a->drive(.5f);a->reset();auto warm=signal("chord",48000,48000,-48000);render(*a,warm,128);
  auto x=signal("chord",48000,128000);auto t=render(*a,x,128);
  std::cout<<id<<','<<run<<",48000,128,128000,"<<t.totalUs/1e6/(128000./48000)*100<<','<<t.worstUs<<','<<t.alloc<<'\n'<<std::flush;
 }
}
