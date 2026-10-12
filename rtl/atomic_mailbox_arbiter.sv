// Arbitration only: all mailbox semantics remain in the generated C++ HLS core.
module dmotor_atomic_mailbox_arbiter #(
    parameter integer MAX_CYCLES=dmotor_timing::MMIO_RESPONSE_MAX_CYCLES
)(
    input wire clk, reset, inhibit,
    input wire [351:0] live_admission,
    // Pulse for canceled frames/stops/faults. Held masks block new work.
    input wire [3:0] invalidate_mask,
    output reg initialized, fault,
    output wire quiescent, cancellation_pending,
    input wire admission_request,
    input wire [63:0] frame_tick,
    output wire admission_done, batch_valid,
    output wire [3:0] batch_mask,
    output wire [31:0] batch_session, batch_sequence,
    output wire [127:0] batch_generations, batch_targets,
    input wire retire_request,
    input wire [31:0] retire_session, retire_sequence,
    output wire retire_done, retire_success,
    input wire host_valid, host_write,
    input wire [31:0] host_address, host_wdata,
    input wire [3:0] host_strobes,
    output wire host_ready,
    output reg host_response_valid,
    input wire host_response_ready,
    output reg host_response_okay,
    output reg [31:0] host_rdata
);
    localparam OP_RESET=0, OP_WRITE=1, OP_READ=2, OP_BEGIN=3, OP_RETIRE=4, OP_INVALIDATE=5;
    localparam OWNER_INIT=0, OWNER_INVALIDATE=1, OWNER_BEGIN=2, OWNER_RETIRE=3, OWNER_HOST=4;
    reg active, begin_seen, retire_seen, poisoned;
    reg [2:0] owner;
    reg [3:0] pending_invalidate;
    reg [31:0] boot_session;
    reg [351:0] captured_admission;
    reg [351:0] previous_admission;
    reg previous_inhibit;
    reg command_valid;
    reg [159:0] command_words;
    reg [2:0] selected_owner;
    wire command_ready, response_valid, response_error, transport_fault;
    wire [511:0] response_words;
    wire [31:0] unused_latency, unused_maximum;
    wire session_changed=initialized && live_admission[31:0]!=boot_session;
    // READY may change as the engine runs. Authorization/configuration changes
    // invalidate work; readiness is checked separately on a begin response.
    wire ownership_changed=initialized &&
        (live_admission[63:0]!=previous_admission[63:0] ||
         live_admission[351:96]!=previous_admission[351:96]);
    wire [3:0] events=invalidate_mask |
        ((ownership_changed || (inhibit && !previous_inhibit)) ? 4'hf : 4'h0);
    wire dirty=|pending_invalidate || |events;
    wire snapshot_changed=live_admission[63:0]!=captured_admission[63:0] ||
                          live_admission[351:96]!=captured_admission[351:96];
    wire usable=initialized && !fault && !transport_fault && !session_changed && !inhibit &&
                !dirty && !poisoned && !snapshot_changed && !response_error;
    // Combinational response qualification also covers cancellation on the edge
    // at which the scheduler samples done. Never register an unqualified success.
    assign admission_done=active && owner==OWNER_BEGIN && response_valid && usable && admission_request;
    assign batch_valid=admission_done && response_words[31:0]!=0 &&
        ((response_words[163:160] & live_admission[67:64])==response_words[163:160]);
    assign batch_mask=response_words[163:160];
    assign batch_session=response_words[127:96];
    assign batch_sequence=response_words[159:128];
    assign batch_targets=response_words[383:256];
    assign batch_generations=response_words[511:384];
    assign retire_done=active && owner==OWNER_RETIRE && response_valid && usable && retire_request;
    assign retire_success=retire_done && response_words[31:0]!=0;
    assign cancellation_pending=dirty || (active && (owner==OWNER_INVALIDATE || poisoned));
    assign quiescent=initialized && !active && !dirty && !fault && !transport_fault;
    wire may_schedule=initialized && !fault && !transport_fault && !session_changed && !inhibit && !dirty;
    wire accept_command=command_valid && command_ready;
    assign host_ready=command_ready && command_valid && selected_owner==OWNER_HOST;
    always @* begin
        command_valid=0; command_words=0; selected_owner=OWNER_INIT;
        if(!reset && !active && !transport_fault)begin
            if(!initialized && !fault && live_admission[31:0]!=0)begin
                command_valid=1; command_words[31:0]=OP_RESET;
            end else if(|pending_invalidate)begin
                command_valid=1; selected_owner=OWNER_INVALIDATE;
                command_words[31:0]=OP_INVALIDATE; command_words[63:32]={28'd0,pending_invalidate};
            end else if(may_schedule)begin
                if(retire_request && !retire_seen)begin
                    command_valid=1; selected_owner=OWNER_RETIRE;
                    command_words={32'd0,32'd1,retire_sequence,retire_session,32'(OP_RETIRE)};
                end else if(admission_request && !begin_seen)begin
                    command_valid=1; selected_owner=OWNER_BEGIN; command_words[31:0]=OP_BEGIN;
                end else if(host_valid && !host_response_valid)begin
                    command_valid=1; selected_owner=OWNER_HOST;
                    command_words={32'd0,28'd0,host_strobes,host_wdata,host_address,
                                   host_write ? 32'(OP_WRITE) : 32'(OP_READ)};
                end
            end
        end
    end
    dmotor_atomic_hls_adapter #(.MAX_CYCLES(MAX_CYCLES)) transport(
        .clk(clk),.reset(reset),.command_valid(command_valid),.command_ready(command_ready),
        .command_words(command_words),.admission_words(live_admission),.command_tick(frame_tick),
        .response_valid(response_valid),.response_ready(active),.response_words(response_words),
        .response_error(response_error),.fault(transport_fault),
        .last_latency(unused_latency),.maximum_latency(unused_maximum));
    always @(posedge clk)begin
        if(reset)begin
            initialized<=0;fault<=0;active<=0;owner<=OWNER_INIT;boot_session<=0;
            begin_seen<=0;retire_seen<=0;poisoned<=0;pending_invalidate<=0;
            captured_admission<=0;previous_admission<=0;previous_inhibit<=0;
            host_response_valid<=0;host_response_okay<=0;host_rdata<=0;
        end else begin
            previous_admission<=live_admission;previous_inhibit<=inhibit;
            pending_invalidate<=pending_invalidate | events;
            if(!admission_request)begin_seen<=0;
            if(!retire_request)retire_seen<=0;
            if(host_response_valid && host_response_ready)host_response_valid<=0;
            if(session_changed || transport_fault)fault<=1;
            if(active && (dirty || inhibit || snapshot_changed))poisoned<=1;
            if(accept_command)begin
                active<=1;owner<=selected_owner;captured_admission<=live_admission;poisoned<=0;
                if(selected_owner==OWNER_INIT)boot_session<=live_admission[31:0];
                if(selected_owner==OWNER_INVALIDATE)pending_invalidate<=events;
                if(selected_owner==OWNER_BEGIN)begin_seen<=1;
                if(selected_owner==OWNER_RETIRE)retire_seen<=1;
            end
            if(active && response_valid)begin
                active<=0;
                if(response_error)fault<=1;
                case(owner)
                OWNER_INIT:begin
                    if(response_error || response_words[31:0]!=1 || snapshot_changed)fault<=1;
                    else initialized<=1;
                end
                OWNER_INVALIDATE:if(response_error || response_words[31:0]!=1)fault<=1;
                OWNER_BEGIN:begin
                    // A successful reservation whose caller vanished must be
                    // explicitly discarded before another begin is possible.
                    if(!usable || !admission_request ||
                       (response_words[31:0]!=0 && !batch_valid))
                        pending_invalidate<=4'hf;
                end
                OWNER_RETIRE:begin
                    // HLS may have updated applied metadata before cancellation
                    // arrived. Fail closed and deny host access until reset, so a
                    // late success cannot be presented as a completed frame.
                    if(!usable || !retire_request)begin fault<=1;pending_invalidate<=4'hf;end
                end
                OWNER_HOST:begin
                    host_response_valid<=1;
                    host_response_okay<=usable && response_words[31:0]!=0;
                    host_rdata<=usable ? response_words[63:32] : 0;
                    if(!usable)pending_invalidate<=4'hf;
                end
                default:fault<=1;
                endcase
            end
        end
    end
endmodule
