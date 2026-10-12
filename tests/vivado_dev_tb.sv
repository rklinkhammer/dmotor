`timescale 1ns/1ps
module vivado_dev_tb;
    reg aclk=0;always #25 aclk=~aclk;
    reg aresetn=0;
    reg [15:0] s_axi_awaddr=0,s_axi_araddr=0;
    reg [2:0] s_axi_awprot=0,s_axi_arprot=0;
    reg s_axi_awvalid=0,s_axi_wvalid=0,s_axi_bready=0,s_axi_arvalid=0,s_axi_rready=0;
    reg [31:0] s_axi_wdata=0;
    reg [3:0] s_axi_wstrb=15;
    wire s_axi_awready,s_axi_wready,s_axi_bvalid,s_axi_arready,s_axi_rvalid;
    wire [1:0] s_axi_bresp,s_axi_rresp;
    wire [31:0] s_axi_rdata;
    wire [3:0] motor_pwm,motor_direction,motor_drive_enable;
    wire motor_inhibit;
    dmotor_vivado_dev dut(.*);
    task step;begin @(posedge aclk);#1;end endtask
    always @(negedge aclk)if(motor_pwm!==0 || motor_direction!==0 || motor_drive_enable!==0 || motor_inhibit!==1)
        $fatal(1,"motor output escaped permanent inhibit");
    task addr(input [15:0] a);begin
        @(negedge aclk);s_axi_awaddr=a;s_axi_awvalid=1;
        @(posedge aclk);while(!s_axi_awready)@(posedge aclk);
        @(negedge aclk);s_axi_awvalid=0;
    end endtask
    task data_word(input [31:0] d,input [3:0] st);begin
        @(negedge aclk);s_axi_wdata=d;s_axi_wstrb=st;s_axi_wvalid=1;
        @(posedge aclk);while(!s_axi_wready)@(posedge aclk);
        @(negedge aclk);s_axi_wvalid=0;
    end endtask
    task bresponse(input [1:0] expected);begin
        wait(s_axi_bvalid);repeat(3)begin step();if(s_axi_bresp!==expected || !s_axi_bvalid)$fatal(1,"write response");end
        @(negedge aclk);s_axi_bready=1;step();@(negedge aclk);s_axi_bready=0;
    end endtask
    task read_word(input [15:0] a,input [31:0] d,input [1:0] resp);begin
        @(negedge aclk);s_axi_araddr=a;s_axi_arvalid=1;
        @(posedge aclk);while(!s_axi_arready)@(posedge aclk);
        @(negedge aclk);s_axi_arvalid=0;
        wait(s_axi_rvalid);repeat(3)begin step();if(s_axi_rdata!==d || s_axi_rresp!==resp || !s_axi_rvalid)$fatal(1,"read %h",a);end
        @(negedge aclk);s_axi_rready=1;step();@(negedge aclk);s_axi_rready=0;
    end endtask
    initial begin
        repeat(3)step();@(negedge aclk);aresetn=1;
        read_word(0,32'h444d4431,0);read_word(4,1,0);read_word(8,1,0);read_word(12,20000000,0);
        addr(16);repeat(4)step();data_word(32'habcdef01,15);bresponse(0);read_word(16,32'habcdef01,0);
        data_word(32'h12345678,15);repeat(4)step();addr(20);bresponse(0);read_word(20,32'h12345678,0);
        addr(16);data_word(0,3);bresponse(2);read_word(16,32'habcdef01,0);
        addr(17);data_word(0,15);bresponse(2);read_word(17,0,2);
        addr(0);data_word(0,15);bresponse(2);read_word(0,32'h444d4431,0);
        read_word(16'hffff,0,2);
        // Read traffic proceeds despite a stalled write response.
        addr(16);data_word(99,15);wait(s_axi_bvalid);read_word(16,99,0);bresponse(0);
        // Reset drains an unmatched address; stale data cannot commit afterward.
        addr(16);@(negedge aclk);aresetn=0;step();@(negedge aclk);aresetn=1;
        read_word(16,0,0);data_word(12,15);repeat(5)step();if(s_axi_bvalid)$fatal(1,"stale address survived reset");
        addr(20);bresponse(0);read_word(20,12,0);
        $display("vivado_dev PASS: split AXI channels, backpressure, invalid accesses, reset and permanent inhibit");$finish;
    end
    initial begin repeat(2000)step();$fatal(1,"timeout");end
endmodule
