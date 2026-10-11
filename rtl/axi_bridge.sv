// Thin AXI4-Lite transport for a serialized ap_ctrl_hs register engine.
// One outstanding read and write. Polls continue while responses are stalled.
// Context is captured atomically with each launch; upstream owns sticky events.
module dmotor_axi_bridge #(
    parameter integer CONTEXT_WIDTH = 1,
    parameter integer ENGINE_TIMEOUT = 1024
)(
    input wire clk, reset,
    input wire [31:0] s_awaddr, input wire s_awvalid, output wire s_awready,
    input wire [31:0] s_wdata, input wire [3:0] s_wstrb,
    input wire s_wvalid, output wire s_wready,
    output reg [1:0] s_bresp, output reg s_bvalid, input wire s_bready,
    input wire [31:0] s_araddr, input wire s_arvalid, output wire s_arready,
    output reg [31:0] s_rdata, output reg [1:0] s_rresp,
    output reg s_rvalid, input wire s_rready,
    input wire [CONTEXT_WIDTH-1:0] context_in,
    output reg [CONTEXT_WIDTH-1:0] context_out,
    output reg engine_start, input wire engine_ready, engine_done,
    output wire engine_reset,
    output reg init_request, bus_valid, bus_write,
    output reg [31:0] bus_address, bus_data, output reg [3:0] bus_strobes,
    input wire [31:0] engine_rdata, input wire engine_error,
    output reg service_done, output reg fault,
    output wire locally_quiesced
);
    reg aw_pending,w_pending,ar_pending;
    reg [31:0] aw_address,w_data,ar_address;
    reg [3:0] w_strobes;
    reg busy,initialized,poll_due,prefer_read;
    reg [31:0] age;
    // Keep accepted channels buffered until their response is deposited.
    assign s_awready=!reset && !fault && !aw_pending && !s_bvalid;
    assign s_wready=!reset && !fault && !w_pending && !s_bvalid;
    assign s_arready=!reset && !fault && !ar_pending && !s_rvalid;
    assign engine_reset=reset || fault;
    assign locally_quiesced=!aw_pending && !w_pending && !ar_pending &&
                           !s_bvalid && !s_rvalid && (!busy || !bus_valid);
    always @(posedge clk) begin
        if(reset) begin
            aw_pending<=0;w_pending<=0;ar_pending<=0;
            aw_address<=0;w_data<=0;w_strobes<=0;ar_address<=0;
            s_bvalid<=0;s_bresp<=0;s_rvalid<=0;s_rresp<=0;s_rdata<=0;
            busy<=0;initialized<=0;poll_due<=0;prefer_read<=0;age<=0;
            engine_start<=0;init_request<=0;bus_valid<=0;bus_write<=0;
            bus_address<=0;bus_data<=0;bus_strobes<=0;
            context_out<=0;service_done<=0;fault<=0;
        end else begin
            service_done<=0;
            if(s_bvalid && s_bready) s_bvalid<=0;
            if(s_rvalid && s_rready) s_rvalid<=0;
            if(s_awvalid && s_awready) begin aw_pending<=1;aw_address<=s_awaddr;end
            if(s_wvalid && s_wready) begin w_pending<=1;w_data<=s_wdata;w_strobes<=s_wstrb;end
            if(s_arvalid && s_arready) begin ar_pending<=1;ar_address<=s_araddr;end
            if(fault) begin
                // Retire accepted complete requests. An unmatched AW/W requires
                // coordinated bus reset; never fabricate its missing half.
                if(aw_pending && w_pending && !s_bvalid) begin
                    s_bvalid<=1;s_bresp<=2;aw_pending<=0;w_pending<=0;
                end
                if(ar_pending && !s_rvalid) begin
                    s_rvalid<=1;s_rresp<=2;s_rdata<=0;ar_pending<=0;
                end
            end else if(busy) begin
                if(engine_ready) engine_start<=0;
                age<=age+1'b1;
                // Deadline wins a simultaneous done; no stale result is accepted.
                if(age==ENGINE_TIMEOUT-1) begin
                    fault<=1;busy<=0;engine_start<=0;
                end else if(engine_done) begin
                    service_done<=1;busy<=0;engine_start<=0;initialized<=1;
                    if(bus_valid) begin
                        poll_due<=1;
                        if(bus_write) begin
                            s_bvalid<=1;s_bresp<=engine_error?2:0;
                            aw_pending<=0;w_pending<=0;prefer_read<=1;
                        end else begin
                            s_rvalid<=1;s_rresp<=engine_error?2:0;s_rdata<=engine_rdata;
                            ar_pending<=0;prefer_read<=0;
                        end
                    end else poll_due<=0;
                end
            end else begin
                busy<=1;age<=0;engine_start<=1;context_out<=context_in;
                init_request<=!initialized;
                bus_valid<=0;bus_write<=0;bus_address<=0;bus_data<=0;bus_strobes<=0;
                if(initialized && !poll_due) begin
                    if(ar_pending && !s_rvalid && (prefer_read || !(aw_pending && w_pending && !s_bvalid))) begin
                        bus_valid<=1;bus_address<=ar_address;
                    end else if(aw_pending && w_pending && !s_bvalid) begin
                        bus_valid<=1;bus_write<=1;bus_address<=aw_address;
                        bus_data<=w_data;bus_strobes<=w_strobes;
                    end
                end
            end
        end
    end
endmodule
