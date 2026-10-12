// Transport only for the measured four-context C++ HLS motor engine.
// Record packing is defined in docs/shared_engine_transport.md.
module dmotor_shared_engine_transport #(
    parameter integer MAX_CYCLES=dmotor_timing::CONTROL_DEADLINE_CYCLES
)(
    input wire clk, reset,
    input wire command_valid,
    output wire command_ready,
    input wire [1119:0] command_record,
    output reg response_valid,
    input wire response_ready,
    output wire [511:0] response_words,
    output reg response_error, fault,
    output reg [31:0] last_latency, maximum_latency
);
    localparam IDLE=0, RUN=1, FINISH=2, RESPONSE=3;
    reg [1:0] state;
    reg [1119:0] record_bank;
    reg [511:0] reply_bank;

    reg [31:0] age;
    reg hls_start;
    wire hls_done,hls_ready;
    wire [1:0] request_address0,request_address1;
    wire [4:0] configuration_address0,configuration_address1;
    wire [3:0] reply_address0,reply_address1;
    wire request_ce0,request_ce1,configuration_ce0,configuration_ce1;
    wire reply_ce0,reply_ce1,reply_we0,reply_we1;
    reg [31:0] request_q0,request_q1,configuration_q0,configuration_q1;
    wire [31:0] reply_d0,reply_d1;
    assign command_ready=state==IDLE && !reset && !fault;
    assign response_words=reply_bank;
    dmotor_mailbox_shared core(.ap_clk(clk),.ap_rst(reset || fault),.ap_start(hls_start),
        .ap_done(hls_done),.ap_ready(hls_ready),.ap_idle(),
        .request_address0(request_address0),.request_ce0(request_ce0),.request_q0(request_q0),
        .request_address1(request_address1),.request_ce1(request_ce1),.request_q1(request_q1),
        .motor(record_bank[0*32 +:32]),.reset(record_bank[32]),
        .tick(record_bank[33]),.authorized(record_bank[34]),.stop(record_bank[35]),
        .capture_fault(record_bank[36]),.negative(record_bank[37]),
        .session(record_bank[2*32 +:32]),.now(record_bank[3*32 +:64]),
        .count(record_bank[5*32 +:64]),.last_edge(record_bank[7*32 +:64]),
        .direction_epoch(record_bank[9*32 +:32]),.invalid_transitions(record_bank[10*32 +:32]),
        .configuration_r_address0(configuration_address0),.configuration_r_ce0(configuration_ce0),
        .configuration_r_q0(configuration_q0),
        .configuration_r_address1(configuration_address1),.configuration_r_ce1(configuration_ce1),
        .configuration_r_q1(configuration_q1),.reply_address0(reply_address0),.reply_ce0(reply_ce0),
        .reply_we0(reply_we0),.reply_d0(reply_d0),.reply_address1(reply_address1),
        .reply_ce1(reply_ce1),.reply_we1(reply_we1),.reply_d1(reply_d1));
    always @(posedge clk) begin
        if(reset)begin
            state<=IDLE;record_bank<=0;reply_bank<=0;age<=0;
            request_q0<=0;request_q1<=0;configuration_q0<=0;configuration_q1<=0;
            response_valid<=0;response_error<=0;fault<=0;hls_start<=0;
            last_latency<=0;maximum_latency<=0;
        end else begin
            // HLS expects one-clock synchronous memory reads, not combinational muxes.
            if(request_ce0)request_q0<=record_bank[(11+request_address0)*32 +:32];
            if(request_ce1)request_q1<=record_bank[(11+request_address1)*32 +:32];
            if(configuration_ce0)configuration_q0<=configuration_address0<20 ? record_bank[(15+configuration_address0)*32 +:32] : 0;
            if(configuration_ce1)configuration_q1<=configuration_address1<20 ? record_bank[(15+configuration_address1)*32 +:32] : 0;
            if(state==RUN || state==FINISH)begin
                if(reply_ce0 && reply_we0)reply_bank[reply_address0*32 +:32]<=reply_d0;
                if(reply_ce1 && reply_we1)reply_bank[reply_address1*32 +:32]<=reply_d1;
            end
            case(state)
            IDLE: if(command_valid && !fault)begin
                record_bank<=command_record;
                reply_bank<=0;age<=0;hls_start<=1;response_error<=0;state<=RUN;
            end
            RUN: begin
                age<=age+1'b1;
                if(hls_ready)hls_start<=0;
                if(age>=MAX_CYCLES-1)begin
                    fault<=1;hls_start<=0;reply_bank<=0;response_error<=1;
                    response_valid<=1;state<=RESPONSE;
                end else if(hls_done)begin
                    hls_start<=0;state<=FINISH;
                    last_latency<=age+1'b1;
                    if(age+1'b1>maximum_latency)maximum_latency<=age+1'b1;
                end
            end
            // Separate finalization lets the last reply write settle before valid.
            FINISH:begin response_valid<=1;state<=RESPONSE;end
            RESPONSE:if(response_ready)begin response_valid<=0;state<=IDLE;end
            default:begin fault<=1;hls_start<=0;state<=IDLE;end
            endcase
        end
    end
endmodule
