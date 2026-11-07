// Parallel PS implementation of the original 35-word engine transaction.
// Hardware scheduler, motor adapter, cleanup and safety remain authoritative.
module dmotor_shared_engine_transport #(
 parameter integer MAX_CYCLES=dmotor_timing::CONTROL_DEADLINE_CYCLES
)(
 input wire clk,reset,command_valid, output wire command_ready,
 input wire [1119:0] command_record, output reg response_valid,
 input wire response_ready, output wire [511:0] response_words,
 output reg response_error,fault, output reg [31:0] last_latency,maximum_latency,
 input wire ps_power_reset,ps_valid,ps_write,
 input wire [15:0] ps_address,input wire [31:0] ps_wdata,input wire [3:0] ps_strobes,
 output wire ps_ready,output reg ps_response_valid,input wire ps_response_ready,
 output reg ps_response_okay,output reg [31:0] ps_rdata
);
 reg pending,attached;
 reg [63:0] ticket,echo_ticket,accepted_ticket;
 reg [1119:0] record_bank;
 reg [511:0] reply_bank,staging;
 reg [17:0] written;
 localparam AGE_BITS=$clog2(MAX_CYCLES+1);
 reg [AGE_BITS-1:0] age;
 wire expired=pending && age>=MAX_CYCLES-1;
 assign command_ready=!reset&&!fault&&!pending&&!response_valid&&attached&&ticket!=64'hffffffffffffffff;
 assign response_words=reply_bank;
 assign ps_ready=!ps_response_valid;
 // Ticket/attach and bus response survive logical engine resets. Physical reset
 // invalidates attachment, so an old userspace reply cannot reattach itself.
 always @(posedge clk)begin
  if(ps_power_reset)begin
   ticket<=0;attached<=0;accepted_ticket<=0;echo_ticket<=0;written<=0;
   ps_response_valid<=0;ps_response_okay<=0;ps_rdata<=0;
   pending<=0;record_bank<=0;reply_bank<=0;staging<=0;age<=0;
   response_valid<=0;response_error<=0;fault<=0;last_latency<=0;maximum_latency<=0;
  end else begin
   if(ps_response_valid&&ps_response_ready)ps_response_valid<=0;
   if(reset)begin
    pending<=0;response_valid<=0;response_error<=0;fault<=0;written<=0;
    reply_bank<=0;age<=0;last_latency<=0;maximum_latency<=0;
   end else begin
    if(response_valid&&response_ready)response_valid<=0;
    if(command_valid&&command_ready)begin
     pending<=1;ticket<=ticket+1'b1;record_bank<=command_record;age<=0;written<=0;
     response_error<=0;
    end
    if(pending)age<=age+1'b1;
    if(expired)begin
     pending<=0;fault<=1;response_valid<=1;response_error<=1;reply_bank<=0;written<=0;
    end
    if(ticket==64'hffffffffffffffff && !pending && !response_valid)fault<=1;
   end
   if(ps_valid&&ps_ready)begin
    ps_response_valid<=1;ps_response_okay<=0;ps_rdata<=0;
    if(!ps_write)begin
     case(ps_address)
      'h4000:begin ps_rdata<=32'h50534631;ps_response_okay<=1;end
      'h4004:begin ps_rdata<=dmotor_timing::CONTROL_PERIOD_US;ps_response_okay<=1;end
      'h4008:begin ps_rdata<={29'd0,attached,fault,pending&&!reset&&!expired};ps_response_okay<=1;end
      'h4010:begin ps_rdata<=ticket[31:0];ps_response_okay<=1;end
      'h4014:begin ps_rdata<=ticket[63:32];ps_response_okay<=1;end
      'h4024:begin ps_rdata<={31'd0,attached};ps_response_okay<=1;end
      'h4028:begin ps_rdata<=accepted_ticket[31:0];ps_response_okay<=1;end
      'h402c:begin ps_rdata<=accepted_ticket[63:32];ps_response_okay<=1;end
      default:for(integer i=0;i<35;i=i+1)if(ps_address=='h4080+4*i)begin
       ps_rdata<=record_bank[i*32+:32];ps_response_okay<=1;
      end
     endcase
    end else if(ps_strobes==15)begin
     if(ps_address=='h4024 && ps_wdata==1 && (!attached || reset) && !pending)begin attached<=1;ps_response_okay<=1;end
     else if(ps_address=='h4018 || ps_address=='h401c || ps_address=='h4020 ||
             (ps_address>='h4140 && ps_address<'h4180 && ps_address[1:0]==0))begin
      // A reset/cancel racing a userspace write is a normal discarded reply.
      // Bus completion is not engine acceptance; PS reads accepted_ticket.
      ps_response_okay<=1;
      if(!reset&&!expired&&!fault&&pending)begin
      case(ps_address)
       'h4018:begin echo_ticket[31:0]<=ps_wdata;written[16]<=1;ps_response_okay<=1;end
       'h401c:begin echo_ticket[63:32]<=ps_wdata;written[17]<=1;ps_response_okay<=1;end
       'h4020:begin
        written<=0;
        if(ps_wdata==1 && (&written) && echo_ticket==ticket)begin
         pending<=0;reply_bank<=staging;response_valid<=1;response_error<=0;
         accepted_ticket<=ticket;last_latency<=age+1'b1;
         if(age+1'b1>maximum_latency)maximum_latency<=age+1'b1;
         ps_response_okay<=1;
        end
       end
       default:for(integer i=0;i<16;i=i+1)if(ps_address=='h4140+4*i)begin
        staging[i*32+:32]<=ps_wdata;written[i]<=1;ps_response_okay<=1;
       end
      endcase
      end
     end
    end
   end
  end
 end
endmodule
