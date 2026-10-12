// Development-only AXI4-Lite endpoint. No register can enable motor drive.
// Separate identity/ABI from both legacy ring and atomic motor-control profiles.
module dmotor_vivado_dev #(
    parameter integer FABRIC_HZ=20000000
)(
    (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S_AXI, ASSOCIATED_RESET aresetn, FREQ_HZ 20000000" *)
    input wire aclk,
    (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *) input wire aresetn,
    input wire [15:0] s_axi_awaddr,
    input wire [2:0] s_axi_awprot,
    input wire s_axi_awvalid, output wire s_axi_awready,
    input wire [31:0] s_axi_wdata, input wire [3:0] s_axi_wstrb,
    input wire s_axi_wvalid, output wire s_axi_wready,
    output reg [1:0] s_axi_bresp, output reg s_axi_bvalid, input wire s_axi_bready,
    input wire [15:0] s_axi_araddr,
    input wire [2:0] s_axi_arprot,
    input wire s_axi_arvalid, output wire s_axi_arready,
    output reg [31:0] s_axi_rdata, output reg [1:0] s_axi_rresp,
    output reg s_axi_rvalid, input wire s_axi_rready,
    output wire [3:0] motor_pwm, motor_direction, motor_drive_enable,
    output wire motor_inhibit
);
    localparam [31:0] IDENTITY=32'h444d4431; // DMD1: development endpoint, not DMB1.
    localparam [15:0] REG_ID=16'h00, REG_VERSION=16'h04, REG_STATUS=16'h08,
        REG_CLOCK=16'h0c, REG_SCRATCH=16'h10, REG_CONFIG=16'h14, REG_CYCLES=16'h18;
    reg aw_pending,w_pending;
    reg [15:0] aw_address;
    reg [31:0] w_data,scratch,dev_config,cycles;
    reg [3:0] w_strobes;
    // Constant assignments survive reset, bus faults and arbitrary register writes.
    assign motor_pwm=0;
    assign motor_direction=0;
    assign motor_drive_enable=0;
    assign motor_inhibit=1;
    assign s_axi_awready=aresetn && !aw_pending && !s_axi_bvalid;
    assign s_axi_wready=aresetn && !w_pending && !s_axi_bvalid;
    assign s_axi_arready=aresetn && !s_axi_rvalid;
    always @(posedge aclk) begin
        if(!aresetn)begin
            aw_pending<=0;w_pending<=0;aw_address<=0;w_data<=0;w_strobes<=0;
            scratch<=0;dev_config<=0;cycles<=0;
            s_axi_bvalid<=0;s_axi_bresp<=0;s_axi_rvalid<=0;s_axi_rresp<=0;s_axi_rdata<=0;
        end else begin
            cycles<=cycles+1'b1;
            if(s_axi_bvalid && s_axi_bready)s_axi_bvalid<=0;
            if(s_axi_rvalid && s_axi_rready)s_axi_rvalid<=0;
            if(s_axi_awvalid && s_axi_awready)begin aw_pending<=1;aw_address<=s_axi_awaddr;end
            if(s_axi_wvalid && s_axi_wready)begin w_pending<=1;w_data<=s_axi_wdata;w_strobes<=s_axi_wstrb;end
            if(aw_pending && w_pending && !s_axi_bvalid)begin
                aw_pending<=0;w_pending<=0;s_axi_bvalid<=1;s_axi_bresp<=2'b10;
                // Full-word aligned accesses only; rejected writes have no effect.
                if(w_strobes==15)begin
                    case(aw_address)
                    REG_SCRATCH:begin scratch<=w_data;s_axi_bresp<=0;end
                    REG_CONFIG:begin dev_config<=w_data;s_axi_bresp<=0;end
                    default:begin end
                    endcase
                end
            end
            if(s_axi_arvalid && s_axi_arready)begin
                s_axi_rvalid<=1;s_axi_rresp<=0;
                case(s_axi_araddr)
                REG_ID:s_axi_rdata<=IDENTITY;
                REG_VERSION:s_axi_rdata<=1;
                REG_STATUS:s_axi_rdata<=1; // bit 0: permanent inhibit; no engine-present bit
                REG_CLOCK:s_axi_rdata<=FABRIC_HZ;
                REG_SCRATCH:s_axi_rdata<=scratch;
                REG_CONFIG:s_axi_rdata<=dev_config;
                REG_CYCLES:s_axi_rdata<=cycles;
                default:begin s_axi_rdata<=0;s_axi_rresp<=2'b10;end
                endcase
            end
        end
    end
endmodule
