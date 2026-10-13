// Revision 0.18 integration endpoint; provisional map documented in controller_axi.md.
module dmotor_controller_axi(
 input wire clk,reset,
 input wire [15:0] awaddr,input wire awvalid,output wire awready,
 input wire [31:0] wdata,input wire [3:0] wstrb,input wire wvalid,output wire wready,
 output wire [1:0] bresp,output wire bvalid,input wire bready,
 input wire [15:0] araddr,input wire arvalid,output wire arready,
 output wire [31:0] rdata,output wire [1:0] rresp,output wire rvalid,input wire rready,
 input wire [3:0] qa,qb,physical_faults,input wire halt_async,
 output wire [3:0] pwm,drive_enable,direction,output wire shared_halt
);
 import dmotor_controller_registers::*;
 wire host_valid,host_write,host_ready,host_response_valid,host_response_ready,host_response_okay;
 wire [31:0] host_address,host_wdata,host_rdata;wire [3:0] host_strobes;
 dmotor_axi_host bus(.*);
 reg local_valid,local_okay,atomic_owner;
 reg [31:0] local_data;
 reg core_reset;
 reg [31:0] session;
 reg [2559:0] configurations,staging;
 reg [79:0] written;
 reg [3:0] configured,enable_request,disable_request,stop,stop_event,refresh_valid,encoder_initialize,age_configure;
 reg [127:0] refresh_sequences;
 reg [31:0] refresh_session;
 reg [63:0] watchdog_timeout_cycles;
 reg [31:0] age_timeout_cycles;
 reg acknowledge_local,acknowledge_external,acknowledge_halt;
 wire [3:0] enabled,ready_mask,stopped,age_faults;
 wire fault,protocol_initialized,controller_busy,controller_blocked;
 reg [3:0] pending_stop;
 wire [2047:0] telemetry;
 wire [255:0] positions,watchdog_fault_times;
 wire [127:0] watchdog_faults;
 reg [2687:0] snapshot_bank;
 reg [31:0] snapshot_sequence;
 wire [127:0] generations={configurations[1920 +:32],configurations[1280 +:32],configurations[640 +:32],configurations[0 +:32]};
 wire [127:0] max_speeds={configurations[1952 +:32],configurations[1312 +:32],configurations[672 +:32],configurations[32 +:32]};
 wire atomic_ready,atomic_valid,atomic_okay;
 wire [31:0] atomic_data;
 wire atomic_select=host_address<IDENTITY && !core_reset && !fault && !shared_halt && !controller_blocked;
 assign host_ready=atomic_select ? atomic_ready : !local_valid && !atomic_owner;
 assign host_response_valid=atomic_owner ? atomic_valid : local_valid;
 assign host_response_okay=atomic_owner ? atomic_okay : local_okay;
 assign host_rdata=atomic_owner ? atomic_data : local_data;
 dmotor_motor_io controller(.clk(clk),.reset(reset),.core_reset(core_reset),
  .inhibit(1'b0),.cancel_frame(1'b0),.halt_async(halt_async),.qa(qa),.qb(qb),.physical_faults(physical_faults),
  .enable_request(enable_request),.disable_request(disable_request),.refresh_valid(refresh_valid),
  .encoder_initialize(encoder_initialize),.refresh_sequences(refresh_sequences),.refresh_session(refresh_session),
  .watchdog_timeout_cycles(watchdog_timeout_cycles),.acknowledge_local(acknowledge_local),
  .acknowledge_external(acknowledge_external),.acknowledge_halt(acknowledge_halt),
  .positions(positions),.watchdog_fault_times(watchdog_fault_times),.watchdog_faults(watchdog_faults),
  .shared_halt(shared_halt),.enabled(enabled),.authorized(configured),.invalidate_mask(stop_event),
  .max_speeds(max_speeds),.configurations(configurations),.stop(stop),.ready_mask(ready_mask),.stopped(stopped),
  .telemetry(telemetry),.protocol_initialized(protocol_initialized),.protocol_fault(),
  .host_valid(host_valid && atomic_select),.host_write(host_write),.host_address(host_address),
  .host_wdata(host_wdata),.host_strobes(host_strobes),.host_ready(atomic_ready),
  .host_response_valid(atomic_valid),.host_response_ready(atomic_owner && host_response_ready),
  .host_response_okay(atomic_okay),.host_rdata(atomic_data),.session(session),.generations(generations),
  .timestamp(),.frame_tick(),.target_inhibit(),.target_apply(),.target_mask(),.target_accepted(),
  .frame_aborted(),.frame_committed(),.committed_duties(),.pwm_duties(),.busy(controller_busy),.fault_inhibit(controller_blocked),.fault(fault),
  .last_latency(),.maximum_latency(),.age_configure(age_configure),.age_acknowledge(4'd0),
  .stop_or_ready(stop),.age_timeout_cycles(age_timeout_cycles),.age_configured(),.age_configure_accepted(),
  .age_armed(),.age_faults(age_faults),.target_ages(),.target_ticks(),.target_fault_times(),.halt_request(),
  .pwm(pwm),.drive_enable(drive_enable),.direction(direction),.applied_duties());
 // Fixed bank/word decoders avoid variable shifts of the entire packed bank.
 // Keep the full address range checks below: low bits alone must never alias.
 wire [1:0] channel=host_address[9:8];
 wire [5:0] word_index=host_address[7:2];
 reg [31:0] configuration_read,snapshot_read;
 integer r,c,w,i;
 always @* begin
  configuration_read=0;snapshot_read=0;
  for(integer bank=0;bank<4;bank=bank+1)
   for(integer word_no=0;word_no<20;word_no=word_no+1)
    configuration_read=configuration_read |
      (configurations[(bank*20+word_no)*32 +:32] &
       {32{channel==bank && word_index==word_no}});
  for(integer word_no=0;word_no<84;word_no=word_no+1)
   snapshot_read=snapshot_read | (snapshot_bank[word_no*32 +:32] &
       {32{host_address==TELEMETRY_BASE+word_no*4}});
 end
 always @(posedge clk)begin
  if(reset)begin
   local_valid<=0;local_okay<=0;local_data<=0;atomic_owner<=0;core_reset<=1;session<=0;
   configurations<=0;staging<=0;written<=0;configured<=0;enable_request<=0;disable_request<=0;stop<=0;stop_event<=0;pending_stop<=0;
   refresh_valid<=0;refresh_sequences<=0;refresh_session<=0;encoder_initialize<=0;age_configure<=0;
   watchdog_timeout_cycles<=dmotor_timing::TARGET_TIMEOUT_DEFAULT_CYCLES;
   age_timeout_cycles<=dmotor_timing::TARGET_TIMEOUT_DEFAULT_CYCLES;
   acknowledge_local<=0;acknowledge_external<=0;acknowledge_halt<=0;snapshot_bank<=0;snapshot_sequence<=0;
  end else begin
   enable_request<=0;disable_request<=0;stop_event<=0;refresh_valid<=0;encoder_initialize<=0;age_configure<=0;
   acknowledge_local<=0;acknowledge_external<=0;acknowledge_halt<=0;
   if(!controller_busy && pending_stop!=0)begin
    stop<=stop|pending_stop;stop_event<=pending_stop;pending_stop<=0;
   end
   if(host_response_valid && host_response_ready)begin local_valid<=0;atomic_owner<=0;end
   if(host_valid && host_ready)begin
    if(atomic_select)atomic_owner<=1;
    else begin
     local_valid<=1;local_okay<=0;local_data<=0;
     if(host_address[1:0]==0 && (!host_write || host_strobes==15))begin
      if(host_address>=CONFIG_BASE && host_address<32'h2400 && word_index<20)begin
       if(host_write)begin
        if(!enabled[channel] && core_reset)begin
         for(c=0;c<4;c=c+1)for(w=0;w<20;w=w+1)
          if(channel==c && word_index==w)begin
           staging[(c*20+w)*32 +:32]<=host_wdata;written[c*20+w]<=1;
          end
         local_okay<=1;
        end
       end else begin local_data<=configuration_read;local_okay<=1;end
      end else if(host_address>=CONFIG_BASE && host_address<32'h2400 && word_index==32 && host_write)begin
       for(c=0;c<4;c=c+1)begin
        if(channel==c && !enabled[c] && core_reset && written[c*20 +:20]==20'hfffff && host_wdata==1 && staging[c*640 +:32]!=0)begin
         configurations[c*640 +:640]<=staging[c*640 +:640];configured[c]<=1;
         written[c*20 +:20]<=0;encoder_initialize[c]<=1;local_okay<=1;
        end
       end
      end else if(host_address>=TELEMETRY_BASE && host_address<32'h3150 && !host_write)begin
       local_data<=snapshot_read;local_okay<=1;
      end else case(host_address)
       IDENTITY:if(!host_write)begin local_data<=PROFILE_ID;local_okay<=1;end
       STATUS:if(!host_write)begin local_data<={9'd0,age_faults,controller_blocked,shared_halt,fault,stopped,ready_mask,configured,enabled};local_okay<=1;end
       SESSION:begin
        if(!host_write)begin local_data<=session;local_okay<=1;end
        else if(core_reset && enabled==0 && host_wdata!=0)begin session<=host_wdata;local_okay<=1;end
       end
       ENABLE:if(host_write && host_wdata<16 && !core_reset && !shared_halt && !fault && protocol_initialized && (host_wdata[3:0]&configured)==host_wdata[3:0] && (host_wdata[3:0]&stop)==0)begin enable_request<=host_wdata[3:0];local_okay<=1;end
       DISABLE:if(host_write && host_wdata<16)begin disable_request<=host_wdata[3:0];local_okay<=1;end
       STOP:if(host_write && host_wdata<16)begin pending_stop<=pending_stop|host_wdata[3:0];local_okay<=1;end
       REFRESH_SESSION:if(host_write)begin refresh_session<=host_wdata;local_okay<=1;end
       CORE_RESET:if(host_write && enabled==0 && host_wdata<=1 && session!=0)begin
        core_reset<=host_wdata[0];if(!host_wdata[0])begin age_configure<=configured;stop<=0;end
        local_okay<=1;
       end
       ACKNOWLEDGE:if(host_write && enabled==0 && host_wdata<8)begin
        acknowledge_local<=host_wdata[0];acknowledge_halt<=host_wdata[1];acknowledge_external<=host_wdata[2];local_okay<=1;
       end
       WATCHDOG_CYCLES:if(host_write && enabled==0 && host_wdata>=(dmotor_timing::WATCHDOG_MIN_US*dmotor_timing::CLOCKS_PER_US) && host_wdata<=(dmotor_timing::WATCHDOG_MAX_US*dmotor_timing::CLOCKS_PER_US))begin watchdog_timeout_cycles<={32'd0,host_wdata};local_okay<=1;end
       TARGET_TIMEOUT_CYCLES:if(host_write && enabled==0 && core_reset && host_wdata>=dmotor_timing::TARGET_TIMEOUT_MIN_CYCLES && host_wdata<=dmotor_timing::TARGET_TIMEOUT_MAX_CYCLES)begin age_timeout_cycles<=host_wdata;local_okay<=1;end
       SNAPSHOT:begin
        if(host_write && host_wdata==1)begin
         for(i=0;i<4;i=i+1)begin
          snapshot_bank[i*672 +:512]<=telemetry[i*512 +:512];
          snapshot_bank[i*672+512 +:64]<=positions[i*64 +:64];
          snapshot_bank[i*672+576 +:32]<=watchdog_faults[i*32 +:32];
          snapshot_bank[i*672+608 +:64]<=watchdog_fault_times[i*64 +:64];
         end
         snapshot_sequence<=snapshot_sequence+1'b1;local_okay<=1;
        end else if(!host_write)begin local_data<=snapshot_sequence;local_okay<=1;end
       end
       REFRESH_BASE,32'h1044,32'h1048,32'h104c:if(host_write)begin
        for(r=0;r<4;r=r+1)if(host_address==REFRESH_BASE+r*4)begin
         refresh_sequences[r*32 +:32]<=host_wdata;refresh_valid[r]<=1;
        end
        local_okay<=1;
       end
       default:begin end
      endcase
     end
    end
   end
  end
 end
endmodule
