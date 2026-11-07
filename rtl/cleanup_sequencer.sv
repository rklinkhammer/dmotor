// One disabled startup cleanup per core-reset release. The reset boundary is
// owned by the caller; no ordinary host/scheduler commands may run while held.
module dmotor_cleanup_sequencer #(
    parameter integer MAX_CYCLES=8*dmotor_timing::CONTROL_DEADLINE_CYCLES
)(
    input wire clk,reset,inhibit,component_fault,invalidate,
    input wire [3:0] enabled,
    input wire [31:0] session,
    input wire [2559:0] configurations,
    input wire mailbox_quiescent,engine_ready,engine_done,
    output wire hold_outputs,engine_override,engine_start,
    output wire complete,pending,admission_ready,
    output reg failed,local_fault,
    output reg [31:0] completed_session
);
    localparam WAIT_SAFE=0,RELEASE=1,START=2,SWEEP=3,COMPLETE=4,OPERATING=5,FAILED=6;
    reg [2:0] state;
    reg motors_clean;
    localparam integer ELAPSED_BITS = MAX_CYCLES > 1 ? $clog2(MAX_CYCLES+64'd1) : 1;
    reg [ELAPSED_BITS-1:0] elapsed;
    reg [31:0] saved_session;
    reg [2559:0] saved_configurations;
    wire changed=session!=saved_session || configurations!=saved_configurations;
    assign hold_outputs=reset || (state!=COMPLETE && state!=OPERATING);
    assign engine_override=state==RELEASE || state==START || state==SWEEP;
    assign engine_start=state==START && engine_ready && !reset && !inhibit && !component_fault && !changed && !invalidate && enabled==0;
    assign admission_ready=!reset && !failed && (state==COMPLETE || state==OPERATING);
    assign complete=!reset && state==COMPLETE && !inhibit && !component_fault && !changed && !invalidate && enabled==0;
    assign pending=!reset && state<COMPLETE;
    always @(posedge clk)begin
        if(reset)begin
            state<=WAIT_SAFE;failed<=0;local_fault<=0;motors_clean<=0;elapsed<=0;saved_session<=0;
            saved_configurations<=0;completed_session<=0;
        end else if(state==WAIT_SAFE)begin
            // HALT/HB qualification may still be settling immediately after
            // CORE_RESET release. PS bounds this wait; no engine work is issued.
            if(component_fault || enabled!=0)begin state<=FAILED;failed<=1;local_fault<=1;end
            else if(!inhibit && session!=0 && mailbox_quiescent)begin
                saved_session<=session;saved_configurations<=configurations;
                state<=RELEASE;elapsed<=0;
            end
        end else if(state!=FAILED)begin
            if(inhibit || component_fault || changed ||
               (state<COMPLETE && (enabled!=0 || invalidate || elapsed>=MAX_CYCLES-1)))begin
                state<=FAILED;failed<=1;completed_session<=0;
                // External inhibition cancels proof, but must not be echoed as
                // a newly attributed local protective fault/HALT request.
                local_fault<=component_fault || changed ||
                    (state<COMPLETE && (enabled!=0 || invalidate || elapsed>=MAX_CYCLES-1));
            end else begin
                if(state<COMPLETE)elapsed<=elapsed+1'b1;
                case(state)
                RELEASE:state<=START;
                START:if(engine_start)state<=SWEEP;
                SWEEP:begin
                    if(engine_done)motors_clean<=1;
                    if((motors_clean || engine_done) && mailbox_quiescent)begin
                        state<=COMPLETE;completed_session<=saved_session;
                    end
                end
                COMPLETE:if(enabled!=0 || invalidate)begin state<=OPERATING;completed_session<=0;end
                default:begin end
                endcase
            end
        end
    end
endmodule
