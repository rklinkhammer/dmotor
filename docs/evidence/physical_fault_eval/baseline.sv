module baseline(input wire clk,reset,core_reset,halt_async,inhibit,ctrl_fault,ctrl_halt,ack_local,ack_external,ack_halt,
input wire [3:0] physical,enable_request,disable_request,authorized,refresh,
input wire [127:0] other_faults,sequences,
input wire [31:0] session,refresh_session,
input wire [63:0] timeout,
output wire [3:0] enabled,external_latched,
output wire [127:0] faults,expected,
output wire [255:0] fault_times,last_refresh,
output wire global_inhibit,shared_halt,coast_req);

(* ASYNC_REG="TRUE" *) reg [3:0] physical_meta,physical_sync;
(* ASYNC_REG="TRUE", DONT_TOUCH="TRUE" *) reg halt_meta,halt_sync;
always @(posedge clk) begin
 if(reset) begin physical_meta<=0;physical_sync<=0;halt_meta<=0;halt_sync<=0;end
 else begin physical_meta<=physical;physical_sync<=physical_meta;halt_meta<=halt_async;halt_sync<=halt_meta;end
end
wire [3:0] requests,safety_inhibits,halt_detected;
wire aggregated;
assign global_inhibit=inhibit || aggregated || |halt_detected || |external_latched || |physical_sync || ctrl_fault || |(enabled & safety_inhibits);
assign shared_halt=aggregated || |requests || ctrl_halt;
assign coast_req=shared_halt || halt_sync || |external_latched;
dmotor_halt_aggregator #(.MOTORS(5)) agg(.clk(clk),.reset(reset),.acknowledge(ack_halt),.requests({ctrl_halt,requests}),.halted(aggregated));

for(genvar m=0;m<4;m=m+1) begin: channel
 dmotor_safety #(.HALT_ALREADY_SYNCHRONIZED(1)) s(.clk(clk),.reset(reset),.halt_async(halt_sync),
 .local_faults((other_faults[m*32+:32] & 32'h18) | (physical_sync[m] ? 32'h4 : 0) | (ctrl_fault ? 32'h2 : 0)),
 .enable_request(enable_request[m]),.disable_request(disable_request[m] || global_inhibit),
 .prerequisites_valid(authorized[m] && !global_inhibit),
 .acknowledge_local(ack_local && !enabled[m]),.acknowledge_external(ack_external && !enabled[m]),
 .timeout_cycles(timeout),.refresh_valid(refresh[m]),.session(session),.refresh_session(refresh_session),
 .refresh_sequence(sequences[m*32+:32]),.enabled(enabled[m]),.inhibit(safety_inhibits[m]),
 .halt_out(requests[m]),.external_latched(external_latched[m]),.faults(faults[m*32+:32]),
 .expected_sequence(expected[m*32+:32]),.timestamp(),.last_refresh(last_refresh[m*64+:64]),
 .first_fault_timestamp(fault_times[m*64+:64]),.halt_detected(halt_detected[m]),.acknowledge_mask(32'hffffffff) );
end
endmodule
