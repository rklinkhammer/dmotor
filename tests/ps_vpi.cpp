#include "dmotor_sw/ps_engine.hpp"
#include <vpi_user.h>
#include <exception>
static dmotor_ps::Engine engine;
static PLI_INT32 execute(PLI_BYTE8*) {
 auto args=vpi_iterate(vpiArgument,vpi_handle(vpiSysTfCall,nullptr));
 auto input=vpi_scan(args);auto output=vpi_scan(args);vpi_free_object(args);
 s_vpi_value value{};value.format=vpiVectorVal;vpi_get_value(input,&value);
 dmotor_ps::Record record{};
 for(unsigned i=0;i<35;++i){
  if(value.value.vector[i].bval){vpi_printf("PS VPI unknown input\n");vpi_control(vpiFinish,1);return 0;}
  record[i]=uint32_t(value.value.vector[i].aval);
 }
 try {
  const auto reply=engine.execute(record);s_vpi_vecval words[16]{};
  for(unsigned i=0;i<16;++i)words[i].aval=PLI_INT32(reply[i]);
  value.value.vector=words;vpi_put_value(output,&value,nullptr,vpiNoDelay);
 }catch(const std::exception& e){vpi_printf("PS VPI error: %s\n",e.what());vpi_control(vpiFinish,1);}
 return 0;
}
static void register_ps(){s_vpi_systf_data data{};data.type=vpiSysTask;data.tfname=const_cast<char*>("$ps_execute");data.calltf=execute;vpi_register_systf(&data);}
extern "C" {void (*vlog_startup_routines[])()={register_ps,nullptr};}
