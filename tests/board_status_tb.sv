module board_status_tb;
 reg clk=0; always #5 clk=~clk;
 reg reset=1,maintenance_async=0,acknowledge=0,drive_authorized=1,hb_healthy=0;
 wire inhibited,inhibit_red,inhibit_green,inhibit_blue,hb_red,hb_green,hb_blue;
 dmotor_board_status #(.RELEASE_CYCLES(4),.BLINK_HALF_CYCLES(2)) dut(.*);
 task ticks(input integer n);repeat(n)begin @(posedge clk);#1;end endtask
 initial begin
  ticks(2);reset=0;ticks(10);if(!inhibited)$fatal(1,"boot must latch inhibit");
  acknowledge=1;ticks(1);acknowledge=0;ticks(1);if(inhibited)$fatal(1,"explicit clear");
  hb_healthy=1;ticks(4);
  if(inhibit_green || hb_green)$fatal(1,"unused green");
  maintenance_async=1;ticks(4);if(!inhibited)$fatal(1,"switch assert");
  acknowledge=1;ticks(4);if(!inhibited)$fatal(1,"held switch cannot ack");
  acknowledge=0;maintenance_async=0;ticks(3);acknowledge=1;ticks(1);acknowledge=0;
  if(!inhibited)$fatal(1,"release debounce bypass");
  ticks(8);if(!inhibited)$fatal(1,"release auto rearm");
  acknowledge=1;ticks(1);acknowledge=0;ticks(1);
  if(inhibited)$fatal(1,"released ack failed");
  wait(dut.blink);#1;if(!inhibit_blue || !hb_blue || inhibit_red || hb_red)$fatal(1,"healthy LEDs");
  drive_authorized=0;hb_healthy=0;#1;
  if(!inhibit_red || !hb_red || inhibit_blue || hb_blue)$fatal(1,"inhibit LEDs");
  $display("board_status PASS: switch, sticky inhibit, release filtering, ACK, LED states");$finish;
 end
endmodule
