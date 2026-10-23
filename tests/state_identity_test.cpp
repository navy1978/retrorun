#include "state_context.h"
#include "state_file.h"
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
 const auto root=fs::temp_directory_path()/("rr-state-context-"+std::to_string(getpid()));
 struct Cleanup{fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}} cleanup{root};
 try{
  fs::create_directories(root);const auto rom=root/"Game.gb",core=root/"core.so",manual=root/"Game.rrstate.auto";
  std::ofstream(rom)<<"ROM";std::ofstream(core)<<"core";std::ofstream(manual)<<"old slot";
  rr::state_context::configure(rom,core);const auto latest=rr::state_context::latestPath(manual);
  require(latest!=manual,"Automatic state collides with manual slot");
  require(state_file::write(latest,"new state",9),"Latest state write failed");
  require(rr::state_context::record(latest,"latest","new state",9),"Latest identity missing");
  require(rr::state_context::canLoad(latest,"new state",9,true),"Verified automatic state rejected");
  require(!rr::state_context::canLoad(latest,"corrupted",9,true),"Same-size corruption accepted");
  require(!rr::state_context::canLoad(manual,"old slot",8,true),"Unknown legacy state autoloaded");
  require(rr::state_context::canLoad(manual,"old slot",8,false),"Explicit legacy load rejected");
  std::ifstream input(manual);std::string bytes((std::istreambuf_iterator<char>(input)),{});require(bytes=="old slot","Manual save overwritten");
  std::ofstream(core)<<"different core";rr::state_context::configure(rom,core);
  require(!rr::state_context::canLoad(latest,"new state",9,false),"Mismatched core state loaded");
  const auto screenshot=root/"capture.png";std::ofstream(screenshot)<<"fixture PNG";
  require(rr::state_context::record(screenshot,"screenshot"),"Screenshot identity failed");
  const auto metadata=rr::state_identity::read(screenshot);
  require(rr::state_identity::get(metadata,"romPath")==fs::canonical(rom) && rr::state_identity::get(metadata,"kind")=="screenshot","Screenshot not associated to actual ROM");
  std::cout<<"Independent latest/manual paths, non-destructive legacy handling, load integrity and screenshot identity passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
