`timescale 1ns/1ps
module shared_engine_transport_tb;
    localparam TEST_DEADLINE=512;
    reg clk=0;always #(0.5e9/dmotor_timing::FABRIC_HZ)clk=~clk;
    reg reset=1,command_valid=0,response_ready=0;
    reg [1119:0] command_record=0;
    wire command_ready,response_valid,response_error,fault;
    wire [511:0] response_words;
    wire [31:0] last_latency,maximum_latency;
    reg [511:0] expected,saved;
    reg [1119:0] first_record;
    integer fd,scan,i,transactions=0;
    reg [31:0] word_value, measured_maximum;
    string vector_path;
    dmotor_shared_engine_transport #(.MAX_CYCLES(TEST_DEADLINE))dut(.*);
    task step;begin @(posedge clk);#1;end endtask
    task launch;begin
        wait(command_ready);@(negedge clk);command_valid=1;step();
        @(negedge clk);command_valid=0;command_record='1;
    end endtask
    task consume;begin
        @(negedge clk);response_ready=1;step();@(negedge clk);response_ready=0;
    end endtask
    initial begin
        if(!$value$plusargs("vectors=%s",vector_path))$fatal(1,"missing vectors");
        fd=$fopen(vector_path,"r");if(!fd)$fatal(1,"cannot open vectors");
        repeat(4)step();@(negedge clk);reset=0;
        scan=$fscanf(fd,"%h",word_value);
        while(scan==1)begin
            command_record[31:0]=word_value;
            for(i=1;i<35;i=i+1)begin
                scan=$fscanf(fd,"%h",word_value);if(scan!=1)$fatal(1,"short request");
                command_record[i*32 +:32]=word_value;
            end
            for(i=0;i<16;i=i+1)begin
                scan=$fscanf(fd,"%h",word_value);if(scan!=1)$fatal(1,"short response");
                expected[i*32 +:32]=word_value;
            end
            if(transactions==0)first_record=command_record;
            launch();wait(response_valid);#1;
            if(response_error || fault)$fatal(1,"transport fault transaction %0d",transactions);
            for(i=0;i<16;i=i+1)
                if(response_words[i*32 +:32]!==expected[i*32 +:32])
                    $fatal(1,"transaction %0d word %0d actual %h expected %h",transactions,i,
                        response_words[i*32 +:32],expected[i*32 +:32]);
            saved=response_words;
            repeat(3)begin step();if(response_words!==saved || !response_valid || command_ready)
                $fatal(1,"backpressure/duplicate acceptance");end
            consume();transactions=transactions+1;
            if(transactions%256==0)$display("shared engine compared %0d transactions",transactions);
            scan=$fscanf(fd,"%h",word_value);
        end
        if(transactions<1600)$fatal(1,"incomplete reference exercise");
        $fclose(fd);
        measured_maximum=maximum_latency;
        // Timeout must return an error and prohibit reuse until a global reset.
        force dut.hls_done=0;command_record=first_record;launch();wait(response_valid);#1;
        if(!fault || !response_error || response_words)$fatal(1,"timeout not fail-closed");
        consume();if(command_ready)$fatal(1,"fault auto-recovered");release dut.hls_done;
        @(negedge clk);reset=1;step();@(negedge clk);reset=0;
        command_record=first_record;launch();wait(response_valid);#1;
        if(fault || response_error || response_words[287:256])$fatal(1,"reset recovery");
        $display("shared_engine_transport PASS: %0d C++ reference transactions, all 16 reply words, immutable input, backpressure, timeout/reset; max invocation latency %0d cycles",transactions,measured_maximum);
        $finish;
    end
    initial begin repeat(TEST_DEADLINE*2000)step();$fatal(1,"test timeout");end
endmodule
