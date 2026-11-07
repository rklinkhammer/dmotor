`timescale 1ns/1ps
module tb;
reg clk=0;always #5 clk=~clk;
reg reset=1,core_reset=0,halt_async=0,inhibit=0,ctrl_fault=0,ctrl_halt=0,ack_local=0,ack_external=0,ack_halt=0;
reg [3:0] physical=0,enable_request=0,disable_request=0,authorized=15,refresh=0;
reg [127:0] other_faults=0,sequences=0;
reg [31:0] session=9,refresh_session=9;
reg [63:0] timeout=20;
wire [3:0] a_enabled;
wire [3:0] a_external_latched;
wire [127:0] a_faults;
wire [127:0] a_expected;
wire [255:0] a_fault_times;
wire [255:0] a_last_refresh;
wire [0:0] a_global_inhibit;
wire [0:0] a_shared_halt;
wire [0:0] a_coast_req;
wire [778:0] a = {a_enabled,a_external_latched,a_faults,a_expected,a_fault_times,a_last_refresh,a_global_inhibit,a_shared_halt,a_coast_req};
wire [3:0] b_enabled;
wire [3:0] b_external_latched;
wire [127:0] b_faults;
wire [127:0] b_expected;
wire [255:0] b_fault_times;
wire [255:0] b_last_refresh;
wire [0:0] b_global_inhibit;
wire [0:0] b_shared_halt;
wire [0:0] b_coast_req;
wire [778:0] b = {b_enabled,b_external_latched,b_faults,b_expected,b_fault_times,b_last_refresh,b_global_inhibit,b_shared_halt,b_coast_req};
wire [3:0] c_enabled;
wire [3:0] c_external_latched;
wire [127:0] c_faults;
wire [127:0] c_expected;
wire [255:0] c_fault_times;
wire [255:0] c_last_refresh;
wire [0:0] c_global_inhibit;
wire [0:0] c_shared_halt;
wire [0:0] c_coast_req;
wire [778:0] c = {c_enabled,c_external_latched,c_faults,c_expected,c_fault_times,c_last_refresh,c_global_inhibit,c_shared_halt,c_coast_req};

baseline a_dut(.clk(clk),.reset(reset),.core_reset(core_reset),.halt_async(halt_async),.inhibit(inhibit),.ctrl_fault(ctrl_fault),.ctrl_halt(ctrl_halt),.ack_local(ack_local),.ack_external(ack_external),.ack_halt(ack_halt),.physical(physical),.enable_request(enable_request),.disable_request(disable_request),.authorized(authorized),.refresh(refresh),.other_faults(other_faults),.sequences(sequences),.session(session),.refresh_session(refresh_session),.timeout(timeout),.enabled(a_enabled),.external_latched(a_external_latched),.faults(a_faults),.expected(a_expected),.fault_times(a_fault_times),.last_refresh(a_last_refresh),.global_inhibit(a_global_inhibit),.shared_halt(a_shared_halt),.coast_req(a_coast_req));
candidate b_dut(.clk(clk),.reset(reset),.core_reset(core_reset),.halt_async(halt_async),.inhibit(inhibit),.ctrl_fault(ctrl_fault),.ctrl_halt(ctrl_halt),.ack_local(ack_local),.ack_external(ack_external),.ack_halt(ack_halt),.physical(physical),.enable_request(enable_request),.disable_request(disable_request),.authorized(authorized),.refresh(refresh),.other_faults(other_faults),.sequences(sequences),.session(session),.refresh_session(refresh_session),.timeout(timeout),.enabled(b_enabled),.external_latched(b_external_latched),.faults(b_faults),.expected(b_expected),.fault_times(b_fault_times),.last_refresh(b_last_refresh),.global_inhibit(b_global_inhibit),.shared_halt(b_shared_halt),.coast_req(b_coast_req));
simplified c_dut(.clk(clk),.reset(reset),.core_reset(core_reset),.halt_async(halt_async),.inhibit(inhibit),.ctrl_fault(ctrl_fault),.ctrl_halt(ctrl_halt),.ack_local(ack_local),.ack_external(ack_external),.ack_halt(ack_halt),.physical(physical),.enable_request(enable_request),.disable_request(disable_request),.authorized(authorized),.refresh(refresh),.other_faults(other_faults),.sequences(sequences),.session(session),.refresh_session(refresh_session),.timeout(timeout),.enabled(c_enabled),.external_latched(c_external_latched),.faults(c_faults),.expected(c_expected),.fault_times(c_fault_times),.last_refresh(c_last_refresh),.global_inhibit(c_global_inhibit),.shared_halt(c_shared_halt),.coast_req(c_coast_req));

integer cycles=0,seed=32'h197403,randval,j,k,negative_mismatches=0;
task step;begin
 @(posedge clk);#1;cycles=cycles+1;
 if(a !== b) begin $display("cycle=%0d physical=%h other=%h ack=%b a=%h b=%h",cycles,physical,other_faults,ack_local,a,b);$fatal(1,"candidate mismatch");end
 if(a !== c) negative_mismatches=negative_mismatches+1;
 @(negedge clk);
end endtask
initial begin
 step();reset=0;enable_request=15;step();enable_request=0;
 // Physical fault followed by a concurrent different fault and acknowledgement.
 physical=1;repeat(4) step();physical=0;repeat(3) step();
 other_faults=8;ack_local=1;step();
 if(negative_mismatches==0) $fatal(1,"simplified clear rule was not rejected");
 other_faults=0;repeat(3) step();ack_halt=1;repeat(3) step();
 ack_external=1;repeat(3) step();
 if(a_enabled!=0) $fatal(1,"automatic enable after clear");
 // All physical masks, release, core-reset hold, and explicit acknowledgement.
 for(k=1;k<16;k=k+1) begin
 reset=1;step();reset=0;ack_local=0;ack_halt=0;ack_external=0;enable_request=15;step();enable_request=0;
 physical=k;repeat(4) step();physical=0;repeat(3) step();core_reset=1;repeat(3) step();core_reset=0;
 if(!a_shared_halt || !a_coast_req || a_enabled!=0) $fatal(1,"protection did not persist");
 ack_local=1;repeat(2) step();ack_halt=1;repeat(2) step();
 end
 // Deterministic randomized differential stimulus, biased toward recovery/re-enable.
 for(k=0;k<30000;k=k+1) begin
 randval=$random(seed);reset=(k%101==0);core_reset=randval[0];
 physical=(k%23<3) ? randval[7:4] : 0;
 other_faults=0;
 for(j=0;j<4;j=j+1) begin
 if(k%29<4) other_faults[j*32+:32]=32'b1 << ((j+k)%5);
 sequences[j*32+:32]=a_expected[j*32+:32];
 end
 halt_async=(k%41==1);inhibit=(k%53==1);ctrl_halt=(k%61==1);ctrl_fault=(k%37==1);
 ack_local=randval[8];ack_external=randval[9];ack_halt=randval[10];
 enable_request=randval[14:11];disable_request=(k%17==0)?randval[18:15]:0;
 refresh=randval[22:19];authorized=randval[26:23];refresh_session=(k%7==0)?8:9;
 timeout=(k%31)+1;step();
 end
 $display("PASS: %0d cycles exact outputs; all physical masks; reset/recovery/watchdog/overlap; simplified rule rejected (%0d mismatching cycles)",cycles,negative_mismatches);$finish;
end
endmodule
