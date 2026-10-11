// Vitis-specific wiring only. Register/queue behavior remains portable C++ HLS.
module dmotor_peripheral_top (
    input wire clk, reset,
    input wire [31:0] s_awaddr, input wire s_awvalid, output wire s_awready,
    input wire [31:0] s_wdata, input wire [3:0] s_wstrb,
    input wire s_wvalid, output wire s_wready,
    output wire [1:0] s_bresp, output wire s_bvalid, input wire s_bready,
    input wire [31:0] s_araddr, input wire s_arvalid, output wire s_arready,
    output wire [31:0] s_rdata, output wire [1:0] s_rresp,
    output wire s_rvalid, input wire s_rready,
    input wire [63:0] timestamp, input wire executor_ready,
    input wire signed [63:0] position, input wire [31:0] max_speed_mrpm,
    input wire upstream_drained, input wire [31:0] generation,
    input wire cancel_valid, input wire [31:0] cancel_cause,
    input wire completion_valid, input wire [31:0] completion_epoch, completion_sequence, completion_detail,
    output wire cancel_ack, completion_ack,
    output reg activated, output reg [31:0] active_epoch, active_sequence, engine_status,
    output reg disable_request, stop_request,
    output wire fault, locally_quiesced,
    input wire halt_async, input wire [31:0] local_faults,
    output wire enabled, inhibit, halt_out, output wire [31:0] safety_faults
);
    wire [451:0] context_sample;
    wire [63:0] ctx_now,ctx_position;
    wire ctx_ready,ctx_drained,ctx_cancel,ctx_complete;
    wire [31:0] ctx_speed,ctx_generation,ctx_cause,ctx_epoch,ctx_sequence,ctx_detail;
    wire [31:0] ctx_safety,ctx_faults,ctx_live,ctx_expected;
    assign {ctx_now,ctx_ready,ctx_position,ctx_speed,ctx_drained,ctx_generation,
            ctx_cancel,ctx_cause,ctx_complete,ctx_epoch,ctx_sequence,ctx_detail,
            ctx_safety,ctx_faults,ctx_live,ctx_expected}=context_sample;
    wire start,ready,done,engine_reset,init,bus_valid,bus_write,serviced;
    wire [31:0] address,data,rd;
    wire [3:0] strobes;
    wire error,rd_valid,error_valid;
    reg [31:0] rd_hold;
    reg error_hold;
    dmotor_axi_bridge #(.CONTEXT_WIDTH(452)) bridge(
      clk,reset,s_awaddr,s_awvalid,s_awready,s_wdata,s_wstrb,s_wvalid,s_wready,
      s_bresp,s_bvalid,s_bready,s_araddr,s_arvalid,s_arready,s_rdata,s_rresp,s_rvalid,s_rready,
      {timestamp,executor_ready,position,max_speed_mrpm,upstream_drained,generation,
       cancel_valid|protect_pending,cancel_valid?cancel_cause:protect_cause,completion_valid,completion_epoch,completion_sequence,completion_detail,
       {29'd0,halt_detected,external_latched,enabled},safety_faults,live_causes,expected_sequence},
      context_sample,start,ready,done,engine_reset,init,bus_valid,bus_write,address,data,strobes,
      rd_valid?rd:rd_hold,(error_valid?error:error_hold)|retire_error,serviced,fault,locally_quiesced);
    wire act,dis,stp,act_valid,dis_valid,stp_valid;
    wire [31:0] epoch,sequence_out,status;
    wire epoch_valid,sequence_valid,status_valid;
    reg act_hold,dis_hold,stp_hold;
    reg [31:0] epoch_hold,sequence_hold,status_hold;
    // Event sources hold their sampled payload until these acknowledgments.
    assign cancel_ack=serviced && ctx_cancel && !init && !fault;
    assign completion_ack=serviced && ctx_complete && !init && !fault;
    dmotor_peripheral hls(
      .ap_clk(clk),.ap_rst(engine_reset),.ap_start(start),.ap_done(done),.ap_ready(ready),.ap_idle(),
      .reset(init),.bus_valid(bus_valid),.write_r(bus_write),.address(address),.data(data),
      .strobes({4'b0,strobes}),.now(ctx_now),.executor_ready(ctx_ready),.position(ctx_position),
      .max_speed_mrpm(ctx_speed),.upstream_drained(ctx_drained),.generation(ctx_generation),
      .cancel_valid(ctx_cancel),.cancel_cause(ctx_cause),.completion_valid(ctx_complete),
      .completion_epoch(ctx_epoch),.completion_sequence(ctx_sequence),.completion_detail(ctx_detail),
      .read_data(rd),.read_data_ap_vld(rd_valid),.bus_error(error),.bus_error_ap_vld(error_valid),
      .engine_status(status),.engine_status_ap_vld(status_valid),.activated(act),.activated_ap_vld(act_valid),
      .active_epoch(epoch),.active_epoch_ap_vld(epoch_valid),.active_sequence(sequence_out),
      .active_sequence_ap_vld(sequence_valid),.disable_request(dis),.disable_request_ap_vld(dis_valid),
      .stop_request(stp),.stop_request_ap_vld(stp_valid),
      .safety_state(ctx_safety),.safety_faults(ctx_faults),.live_faults(ctx_live),.expected_refresh(ctx_expected),
      .safety_action(action_raw),.safety_action_ap_vld(action_valid),
      .safety_argument(argument_raw),.safety_argument_ap_vld(argument_valid),
      .refresh_session_out(refresh_raw),.refresh_session_out_ap_vld(refresh_valid),
      .timeout_cycles(timeout_raw),.timeout_cycles_ap_vld(timeout_valid),
      .configuration_valid(config_raw),.configuration_valid_ap_vld(config_valid));
    wire [31:0] action_raw,argument_raw,refresh_raw;
    wire [63:0] timeout_raw;
    wire action_valid,argument_valid,refresh_valid,timeout_valid,config_raw,config_valid;
    reg [31:0] action_hold,argument_hold,refresh_hold;
    reg [63:0] timeout_hold;
    reg config_hold;
    wire [31:0] action_now=action_valid?action_raw:action_hold;
    wire [31:0] argument_now=argument_valid?argument_raw:argument_hold;
    wire [31:0] refresh_now=refresh_valid?refresh_raw:refresh_hold;
    wire [63:0] timeout_now=timeout_valid?timeout_raw:timeout_hold;
    wire config_now=config_valid?config_raw:config_hold;
    wire [31:0] epoch_now=epoch_valid?epoch:epoch_hold;
    wire [31:0] status_now=status_valid?status:status_hold;
    wire external_latched,halt_detected;
    wire [31:0] expected_sequence;
    wire [63:0] safety_time,last_refresh;
    wire [31:0] live_causes=local_faults | (fault?32'h2:32'd0);
    wire expired=enabled && safety_time-last_refresh>=timeout_now;
    wire enable_allowed=config_now && !cancel_valid && !protect_pending && !enabled && !halt_detected && !external_latched &&
                        safety_faults==0 && live_causes==0 && (status_now&7)==2 && epoch_now!=0;
    wire refresh_allowed=enabled && !cancel_valid && !protect_pending && !expired && !inhibit && refresh_now==epoch_now &&
                         expected_sequence!=0 && argument_now==expected_sequence;
    wire retire_error=(action_now==1 && !enable_allowed) ||
                      (action_now==2 && !refresh_allowed) ||
                      (action_now==3 && ((argument_now&live_causes)!=0 || (argument_now&~safety_faults)!=0)) ||
                      (action_now==4 && halt_detected);
    wire safety_commit=done && !init && !fault && !(error_valid?error:error_hold) && !retire_error;
    reg protect_pending,protect_seen;
    reg [31:0] protect_cause;
    wire protection_active=(|safety_faults)||external_latched||halt_detected;
    dmotor_safety safety(
      .clk(clk),.reset(reset),.halt_async(halt_async),.local_faults(live_causes),
      .enable_request(safety_commit && action_now==1),
      .disable_request(done && (dis_valid?dis:dis_hold)),.prerequisites_valid(enable_allowed),
      .acknowledge_local(safety_commit && action_now==3),
      .acknowledge_mask(argument_now),.acknowledge_external(safety_commit && action_now==4),
      .timeout_cycles(timeout_now),.refresh_valid(safety_commit && action_now==2),
      .session(epoch_now),.refresh_session(refresh_now),.refresh_sequence(argument_now),
      .enabled(enabled),.inhibit(inhibit),.halt_out(halt_out),.external_latched(external_latched),
      .faults(safety_faults),.expected_sequence(expected_sequence),.timestamp(safety_time),
      .last_refresh(last_refresh),.first_fault_timestamp(),.halt_detected(halt_detected));
    always @(posedge clk) begin
        if(reset) begin
            action_hold<=0;argument_hold<=0;refresh_hold<=0;timeout_hold<=0;config_hold<=0;
            protect_pending<=0;protect_seen<=0;protect_cause<=0;
        end else begin
            if(action_valid) action_hold<=action_raw;
            if(argument_valid) argument_hold<=argument_raw;
            if(refresh_valid) refresh_hold<=refresh_raw;
            if(timeout_valid) timeout_hold<=timeout_raw;
            if(config_valid) config_hold<=config_raw;
            if(cancel_ack) protect_pending<=0;
            if(!protection_active) protect_seen<=0;
            if(protection_active && !protect_seen) begin
                protect_seen<=1;protect_pending<=1;
                protect_cause<=safety_faults!=0?safety_faults:32'h20000000;
            end
        end
    end
    always @(posedge clk) begin
        if(reset) begin
            rd_hold<=0;error_hold<=0;act_hold<=0;dis_hold<=0;stp_hold<=0;
            epoch_hold<=0;sequence_hold<=0;status_hold<=0;
            activated<=0;disable_request<=0;stop_request<=0;active_epoch<=0;active_sequence<=0;engine_status<=0;
        end else begin
            if(rd_valid) rd_hold<=rd;
            if(error_valid) error_hold<=error;
            if(act_valid) act_hold<=act;
            if(dis_valid) dis_hold<=dis;
            if(stp_valid) stp_hold<=stp;
            if(epoch_valid) epoch_hold<=epoch;
            if(sequence_valid) sequence_hold<=sequence_out;
            if(status_valid) status_hold<=status;
            activated<=0;disable_request<=0;stop_request<=0;
            if(serviced && !fault) begin
                // A protective event arriving during HLS execution masks stale
                // activation. Accounting is canceled in a subsequent transaction.
                activated<=act_hold && executor_ready && !cancel_valid && !inhibit;
                disable_request<=dis_hold;
                stop_request<=stp_hold && !cancel_valid;
                active_epoch<=epoch_hold;active_sequence<=sequence_hold;engine_status<=status_hold;
            end
        end
    end
endmodule
