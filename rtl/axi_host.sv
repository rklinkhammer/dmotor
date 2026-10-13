// AXI4-Lite to serialized host interface, one read and one write buffered.
module dmotor_axi_host(
 input wire clk,reset,
 input wire [15:0] awaddr,input wire awvalid,output wire awready,
 input wire [31:0] wdata,input wire [3:0] wstrb,input wire wvalid,output wire wready,
 output reg [1:0] bresp,output reg bvalid,input wire bready,
 input wire [15:0] araddr,input wire arvalid,output wire arready,
 output reg [31:0] rdata,output reg [1:0] rresp,output reg rvalid,input wire rready,
 output reg host_valid,host_write,output reg [31:0] host_address,host_wdata,
 output reg [3:0] host_strobes,input wire host_ready,
 input wire host_response_valid,host_response_okay,input wire [31:0] host_rdata,
 output wire host_response_ready
);
 reg aw_full,w_full,ar_full,waiting,prefer_read;
 reg [15:0] aw_bank,ar_bank;
 reg [31:0] w_bank;reg [3:0] st_bank;
 assign awready=!reset && !aw_full && !bvalid;
 assign wready=!reset && !w_full && !bvalid;
 assign arready=!reset && !ar_full && !rvalid;
 assign host_response_ready=waiting;
 always @(posedge clk)begin
  if(reset)begin
   aw_full<=0;w_full<=0;ar_full<=0;waiting<=0;prefer_read<=0;
   aw_bank<=0;ar_bank<=0;w_bank<=0;st_bank<=0;
   bvalid<=0;bresp<=0;rvalid<=0;rresp<=0;rdata<=0;
   host_valid<=0;host_write<=0;host_address<=0;host_wdata<=0;host_strobes<=0;
  end else begin
   if(bvalid && bready)bvalid<=0;
   if(rvalid && rready)rvalid<=0;
   if(awvalid && awready)begin aw_bank<=awaddr;aw_full<=1;end
   if(wvalid && wready)begin w_bank<=wdata;st_bank<=wstrb;w_full<=1;end
   if(arvalid && arready)begin ar_bank<=araddr;ar_full<=1;end
   if(host_valid && host_ready)begin host_valid<=0;waiting<=1;end
   if(waiting && host_response_valid)begin
    waiting<=0;
    if(host_write)begin
     bvalid<=1;bresp<=host_response_okay?0:2;aw_full<=0;w_full<=0;prefer_read<=1;
    end else begin
     rvalid<=1;rresp<=host_response_okay?0:2;rdata<=host_rdata;ar_full<=0;prefer_read<=0;
    end
   end
   if(!host_valid && !waiting)begin
    if(ar_full && !rvalid && (prefer_read || !(aw_full && w_full && !bvalid)))begin
     host_valid<=1;host_write<=0;host_address<={16'd0,ar_bank};host_wdata<=0;host_strobes<=15;
    end else if(aw_full && w_full && !bvalid)begin
     host_valid<=1;host_write<=1;host_address<={16'd0,aw_bank};host_wdata<=w_bank;host_strobes<=st_bank;
    end
   end
  end
 end
endmodule
