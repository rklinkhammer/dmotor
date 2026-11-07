// EXPERIMENTAL PSE1 register map. Same board-facing ports; incompatible MMIO.
// No automatic recovery; fault clearance requires external reset in this prototype.
module ps_controller_axi #(
 parameter PWM_CYCLES=1000, CAPTURE_MAX=200000, DUTY_TIMEOUT=400000,
 parameter AUTH_TIMEOUT=2000000, HB_STARTUP=20000000, HB_TIMEOUT=1000000,
 parameter HB_QUALIFY=10000000, HB_MIN=20000
)(
 input wire clk,reset,
 input wire [15:0] awaddr,input wire awvalid,output wire awready,
 input wire [31:0] wdata,input wire [3:0] wstrb,input wire wvalid,output wire wready,
 output wire [1:0] bresp,output wire bvalid,input wire bready,
 input wire [15:0] araddr,input wire arvalid,output wire arready,
 output wire [31:0] rdata,output wire [1:0] rresp,output wire rvalid,input wire rready,
 input wire [3:0] qa,qb,physical_faults,input wire halt_async,hb_async,
 output wire [3:0] pwm,direction,output reg [3:0] drive_enable,
 output wire shared_halt,coast_req
);
 wire host_valid,host_write,host_ready,host_response_valid,host_response_ready,host_response_okay;
 wire [31:0] host_address,host_wdata,host_rdata;wire [3:0] host_strobes;
 dmotor_axi_host bus(.*);
 reg response_valid,response_okay;reg [31:0] response_data;
 assign host_ready=!response_valid;
 assign host_response_valid=response_valid;assign host_response_okay=response_okay;
 assign host_rdata=response_data;
 (* ASYNC_REG="TRUE" *) reg halt_meta,halt_sync;
 (* ASYNC_REG="TRUE" *) reg [3:0] physical_meta,physical_sync;
 always @(posedge clk)begin
  if(reset)begin halt_meta<=0;halt_sync<=0;physical_meta<=0;physical_sync<=0;end
  else begin halt_meta<=halt_async;halt_sync<=halt_meta;physical_meta<=physical_faults;physical_sync<=physical_meta;end
 end
 wire monitor_fault,monitor_healthy,monitor_qualified;
 dmotor_monitor_heartbeat #(.STARTUP_CYCLES(HB_STARTUP),.TIMEOUT_CYCLES(HB_TIMEOUT),
  .QUALIFY_CYCLES(HB_QUALIFY),.MIN_EDGE_CYCLES(HB_MIN)) monitor(
  .clk(clk),.reset(reset),.core_reset(1'b0),.hb_async(hb_async),.acknowledge(1'b0),
  .fault(monitor_fault),.healthy(monitor_healthy),.qualified(monitor_qualified));
 reg [63:0] now,arm_tick,auth_tick,last_duty_tick,snapshot_tick,pending_tick;
 reg [31:0] session,refresh_sequence,snapshot_token,used_token,accepted_sequence,applied_sequence;
 reg armed,fault,pending;
 localparam PHASE_BITS=PWM_CYCLES>1?$clog2(PWM_CYCLES):1;
 reg [PHASE_BITS-1:0] phase;
 wire boundary=phase==PWM_CYCLES-1;
 reg [127:0] staged_duty,pending_duty,committed_duty;
 reg [31:0] staged_session,staged_sequence,staged_token;
 reg [6:0] written;
 wire [255:0] positions,counts,edges;
 // Match the collector's one-cycle latency relative to the decoder outputs.
 reg [255:0] collected_positions;
 always @(posedge clk)begin
  if(reset)collected_positions<=0;
  else collected_positions<=positions;
 end
 wire [127:0] epochs,invalids;
 wire [3:0] negative,capture_fault,range_fault,raw_enable;
 wire [127:0] actual_duty;
 reg [255:0] snap_positions,snap_counts,snap_edges;
 reg [127:0] snap_epochs,snap_invalids;
 reg [3:0] snap_negative;
 wire expired=armed && ((now-auth_tick>=AUTH_TIMEOUT)||(now-last_duty_tick>=DUTY_TIMEOUT));
 // Reserve one PWM period for the existing reversal zero-drive interval.
 localparam REQUEST_MAX=CAPTURE_MAX-PWM_CYCLES;
 wire bad_pending=pending && now-pending_tick>=REQUEST_MAX;
 wire danger=halt_sync || |physical_sync || monitor_fault || |capture_fault || |invalids || expired || bad_pending;
 wire inhibit=reset || !armed || fault || danger || !monitor_qualified;
 assign shared_halt=fault||danger;
 assign coast_req=shared_halt;
 wire [127:0] requested=(pending && boundary && !inhibit)?pending_duty:committed_duty;
 wire duty_valid=$signed(staged_duty[0+:32])>=-65536 && $signed(staged_duty[0+:32])<=65536 &&
  $signed(staged_duty[32+:32])>=-65536 && $signed(staged_duty[32+:32])<=65536 &&
  $signed(staged_duty[64+:32])>=-65536 && $signed(staged_duty[64+:32])<=65536 &&
  $signed(staged_duty[96+:32])>=-65536 && $signed(staged_duty[96+:32])<=65536;
 genvar m;
 generate for(m=0;m<4;m=m+1)begin: channel
  wire ev,en;wire [63:0] et;
  qcencoder decoder(.clk(clk),.reset(reset),.initialize(1'b0),.qa(qa[m]),.qb(qb[m]),
   .invert(1'b0),.mode(2'd2),.initial_position(64'sd0),.position(positions[m*64+:64]),
   .invalid_transitions(invalids[m*32+:32]),.range_fault(range_fault[m]),.event_valid(ev),
   .event_negative(en),.timestamp(),.event_timestamp(et));
  dmotor_encoder_snapshot capture(.clk(clk),.reset(reset),.event_valid(ev),.event_negative(en),
   .event_timestamp(et),.encoder_invalid_transitions(invalids[m*32+:32]),
   .encoder_range_fault(range_fault[m]),.count(counts[m*64+:64]),.last_edge(edges[m*64+:64]),
   .direction_epoch(epochs[m*32+:32]),.invalid_transitions(),.negative(negative[m]),.capture_fault(capture_fault[m]));
  dmotor_pwm #(.PERIOD_CYCLES(PWM_CYCLES)) output_pwm(.clk(clk),.reset(reset),.inhibit(inhibit),
   .requested_duty_q16(requested[m*32+:32]),.pwm(pwm[m]),.drive_enable(raw_enable[m]),
   .direction(direction[m]),.reversing(),.applied_duty_q16(actual_duty[m*32+:32]),
   .timestamp(),.application_timestamp());
 end endgenerate
 always @(posedge clk)begin
  if(reset)begin
   now<=0;phase<=0;armed<=0;fault<=0;pending<=0;session<=0;refresh_sequence<=1;
   arm_tick<=0;auth_tick<=0;last_duty_tick<=0;snapshot_tick<=0;pending_tick<=0;
   snapshot_token<=0;used_token<=0;accepted_sequence<=0;applied_sequence<=0;
   staged_duty<=0;pending_duty<=0;committed_duty<=0;written<=0;
   staged_session<=0;staged_sequence<=0;staged_token<=0;
   snap_positions<=0;snap_counts<=0;snap_edges<=0;snap_epochs<=0;snap_invalids<=0;snap_negative<=0;
   response_valid<=0;response_okay<=0;response_data<=0;drive_enable<=0;
  end else begin
   now<=now+1'b1;phase<=boundary?0:phase+1'b1;drive_enable<=raw_enable;
   if(response_valid && host_response_ready)response_valid<=0;
   if(danger)begin fault<=1;armed<=0;pending<=0;committed_duty<=0;written<=0;end
   else if(pending && boundary && !inhibit)begin
    committed_duty<=pending_duty;pending<=0;applied_sequence<=accepted_sequence;last_duty_tick<=pending_tick;
   end
   if(host_valid && host_ready)begin
    response_valid<=1;response_okay<=0;response_data<=0;
    if(!host_write)begin
     case(host_address)
      0:begin response_data<=32'h50534531;response_okay<=1;end
      4:begin response_data<={28'd0,pending,monitor_qualified,fault,armed};response_okay<=1;end
      8:begin response_data<=session;response_okay<=1;end
      16:begin response_data<=snapshot_token;response_okay<=1;end
      20:begin response_data<=snapshot_tick[31:0];response_okay<=1;end
      24:begin response_data<=snapshot_tick[63:32];response_okay<=1;end
      28:begin response_data<=accepted_sequence;response_okay<=1;end
      32:begin response_data<=applied_sequence;response_okay<=1;end
      36:begin response_data<={28'd0,snap_negative};response_okay<=1;end
      default:begin
       for(integer i=0;i<4;i=i+1)begin
        if(host_address==128+i*32)begin response_data<=snap_positions[i*64+:32];response_okay<=1;end
        if(host_address==132+i*32)begin response_data<=snap_positions[i*64+32+:32];response_okay<=1;end
        if(host_address==136+i*32)begin response_data<=snap_counts[i*64+:32];response_okay<=1;end
        if(host_address==140+i*32)begin response_data<=snap_counts[i*64+32+:32];response_okay<=1;end
        if(host_address==144+i*32)begin response_data<=snap_edges[i*64+:32];response_okay<=1;end
        if(host_address==148+i*32)begin response_data<=snap_edges[i*64+32+:32];response_okay<=1;end
        if(host_address==152+i*32)begin response_data<=snap_epochs[i*32+:32];response_okay<=1;end
        if(host_address==156+i*32)begin response_data<=snap_invalids[i*32+:32];response_okay<=1;end
        if(host_address==256+i*4)begin response_data<=actual_duty[i*32+:32];response_okay<=1;end
       end
      end
     endcase
    end else if(host_strobes==15)begin
     case(host_address)
      8:if(!armed&&!fault&&!danger&&monitor_qualified&&host_wdata>session)begin
       session<=host_wdata;armed<=1;auth_tick<=now;arm_tick<=now;last_duty_tick<=now;
       refresh_sequence<=1;accepted_sequence<=0;applied_sequence<=0;used_token<=snapshot_token;
       written<=0;pending<=0;committed_duty<=0;response_okay<=1;
      end
      12:if(armed&&!inhibit&&host_wdata==refresh_sequence&&refresh_sequence!=0)begin
       auth_tick<=now;refresh_sequence<=refresh_sequence+1'b1;response_okay<=1;
      end
      16:if(snapshot_token!=32'hffffffff)begin
       snapshot_token<=snapshot_token+1'b1;snapshot_tick<=now;
       snap_positions<=collected_positions;snap_counts<=counts;snap_edges<=edges;
       snap_epochs<=epochs;snap_invalids<=invalids;snap_negative<=negative;response_okay<=1;
      end
      64:begin staged_session<=host_wdata;written[0]<=1;response_okay<=1;end
      68:begin staged_sequence<=host_wdata;written[1]<=1;response_okay<=1;end
      72:begin staged_token<=host_wdata;written[2]<=1;response_okay<=1;end
      76:begin staged_duty[0+:32]<=host_wdata;written[3]<=1;response_okay<=1;end
      80:begin staged_duty[32+:32]<=host_wdata;written[4]<=1;response_okay<=1;end
      84:begin staged_duty[64+:32]<=host_wdata;written[5]<=1;response_okay<=1;end
      88:begin staged_duty[96+:32]<=host_wdata;written[6]<=1;response_okay<=1;end
      92:begin
       written<=0;
       if(host_wdata==1 && written==127 && !pending && !inhibit && staged_session==session &&
          staged_sequence>accepted_sequence && staged_token==snapshot_token && staged_token>used_token &&
          snapshot_tick>=arm_tick && now-snapshot_tick<REQUEST_MAX && duty_valid)begin
        pending_duty<=staged_duty;pending_tick<=snapshot_tick;pending<=1;
        accepted_sequence<=staged_sequence;used_token<=staged_token;response_okay<=1;
       end
      end
      96:if(host_wdata==1)begin armed<=0;pending<=0;committed_duty<=0;written<=0;response_okay<=1;end
      100:if(host_wdata==1)begin fault<=1;armed<=0;pending<=0;committed_duty<=0;written<=0;response_okay<=1;end
      default:begin end
     endcase
    end
   end
  end
 end
endmodule
