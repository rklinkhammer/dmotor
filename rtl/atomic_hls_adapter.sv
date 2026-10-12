// Transport for the generated dmotor_atomic_mailbox ap_ctrl_hs + dual-port
// ap_memory interfaces. Protocol semantics remain in C++ HLS.
module dmotor_atomic_hls_adapter #(
    parameter integer MAX_CYCLES=dmotor_timing::MMIO_RESPONSE_MAX_CYCLES
)(
    input wire clk, reset,
    input wire command_valid,
    output wire command_ready,
    input wire [159:0] command_words,
    input wire [351:0] admission_words,
    input wire [63:0] command_tick,
    output reg response_valid,
    input wire response_ready,
    output wire [511:0] response_words,
    output reg response_error, fault,
    output reg [31:0] last_latency, maximum_latency
);
    localparam IDLE=0, RUN=1, FINISH=2, RESPONSE=3;
    reg [1:0] state;
    reg [159:0] request_bank;
    reg [351:0] admission_bank;
    reg [511:0] reply_bank;
    reg [63:0] tick_bank;
    reg [31:0] age;
    reg hls_start;
    wire hls_done,hls_ready;
    wire [2:0] request_address0,request_address1;
    wire [3:0] admission_address0,admission_address1,reply_address0,reply_address1;
    wire request_ce0,request_ce1,admission_ce0,admission_ce1;
    wire reply_ce0,reply_ce1,reply_we0,reply_we1;
    reg [31:0] request_q0,request_q1,admission_q0,admission_q1;
    wire [31:0] reply_d0,reply_d1;
    assign command_ready=state==IDLE && !reset && !fault;
    assign response_words=reply_bank;
    dmotor_atomic_mailbox core(.ap_clk(clk),.ap_rst(reset || fault),.ap_start(hls_start),
        .ap_done(hls_done),.ap_ready(hls_ready),.ap_idle(),
        .request_address0(request_address0),.request_ce0(request_ce0),.request_q0(request_q0),
        .request_address1(request_address1),.request_ce1(request_ce1),.request_q1(request_q1),
        .admission_address0(admission_address0),.admission_ce0(admission_ce0),.admission_q0(admission_q0),
        .admission_address1(admission_address1),.admission_ce1(admission_ce1),.admission_q1(admission_q1),
        .tick(tick_bank),.reply_address0(reply_address0),.reply_ce0(reply_ce0),
        .reply_we0(reply_we0),.reply_d0(reply_d0),.reply_address1(reply_address1),
        .reply_ce1(reply_ce1),.reply_we1(reply_we1),.reply_d1(reply_d1));
    always @(posedge clk) begin
        if(reset)begin
            state<=IDLE;request_bank<=0;admission_bank<=0;reply_bank<=0;tick_bank<=0;age<=0;
            request_q0<=0;request_q1<=0;admission_q0<=0;admission_q1<=0;
            response_valid<=0;response_error<=0;fault<=0;hls_start<=0;
            last_latency<=0;maximum_latency<=0;
        end else begin
            // HLS expects one-clock synchronous memory reads, not combinational muxes.
            if(request_ce0)request_q0<=request_address0<5 ? request_bank[request_address0*32 +:32] : 0;
            if(request_ce1)request_q1<=request_address1<5 ? request_bank[request_address1*32 +:32] : 0;
            if(admission_ce0)admission_q0<=admission_address0<11 ? admission_bank[admission_address0*32 +:32] : 0;
            if(admission_ce1)admission_q1<=admission_address1<11 ? admission_bank[admission_address1*32 +:32] : 0;
            if(state==RUN || state==FINISH)begin
                if(reply_ce0 && reply_we0)reply_bank[reply_address0*32 +:32]<=reply_d0;
                if(reply_ce1 && reply_we1)reply_bank[reply_address1*32 +:32]<=reply_d1;
            end
            case(state)
            IDLE: if(command_valid && !fault)begin
                request_bank<=command_words;admission_bank<=admission_words;tick_bank<=command_tick;
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
