// Serialized adaptation to the PS fixed-point motor engine. No motor arithmetic here.
module dmotor_motor_engine_adapter(

    input wire ps_power_reset,ps_valid,ps_write,
    input wire [15:0] ps_address,input wire [31:0] ps_wdata,input wire [3:0] ps_strobes,
    output wire ps_ready,ps_response_valid,input wire ps_response_ready,
    output wire ps_response_okay,output wire [31:0] ps_rdata,
    input wire clk, reset,
    input wire start,
    // Internal primitive: caller must hold independent output inhibition and
    // reset/cancel old work before requesting. Completion is a pulse, not a
    // system recovery attestation. No motor is enabled or ticked by this sweep.
    input wire cleanup_start,
    output reg cleanup_busy, cleanup_done,
    output wire ready,
    output reg done, fault,
    input wire [1:0] motor,
    input wire [31:0] session, generation, target,
    input wire [63:0] tick,
    input wire [255:0] snapshot,
    input wire apply_target,
    input wire [3:0] enabled, stop,
    input wire [2559:0] configurations,
    output reg [3:0] ready_mask, stopped,
    output reg [1:0] result_motor,
    output reg [31:0] result_session, result_generation,
    output reg [63:0] result_tick,
    output reg signed [31:0] result_duty,
    output reg [2047:0] telemetry
);
    localparam IDLE=0,SEND=1,WAIT_REPLY=2,FINISH=3;
    localparam RESET_CONTEXT=0,STAGE_SESSION=1,STAGE_GENERATION=2,STAGE_SEQUENCE=3,
        STAGE_SPEED=4,COMMIT_TARGET=5,TICK=6;
    reg [1:0] state;
    reg [2:0] operation;
    reg [3:0] initialized,stop_sent;
    reg [127:0] sequences,active_targets;
    reg [31:0] selected_sequence,saved_target;
    reg saved_apply,saved_stop;
    reg [255:0] saved_snapshot;
    reg [639:0] saved_config;
    reg [1119:0] record;
    reg [2559:0] cleanup_configurations;
    wire transport_ready,response_valid,response_error,transport_fault;
    wire [511:0] reply;
    wire [31:0] ignored_latency,ignored_max;
    assign ready=state==IDLE && !reset && !fault;
    always @* begin
        record=0;
        record[31:0]={30'd0,result_motor};
        record[32]=operation==RESET_CONTEXT;
        record[33]=operation==TICK;
        record[34]=!cleanup_busy;
        record[35]=saved_stop && !stop_sent[result_motor] && operation==TICK;
        record[36]=saved_snapshot[193];
        record[37]=saved_snapshot[192];
        record[2*32 +:32]=result_session;
        record[3*32 +:64]=result_tick;
        record[5*32 +:192]=saved_snapshot[191:0];
        record[15*32 +:640]=saved_config;
        if(operation>=STAGE_SESSION && operation<=STAGE_SPEED)begin
            record[11*32 +:32]=1;
            record[12*32 +:32]=operation-STAGE_SESSION;
            case(operation)
            STAGE_SESSION:record[13*32 +:32]=result_session;
            STAGE_GENERATION:record[13*32 +:32]=result_generation;
            STAGE_SEQUENCE:record[13*32 +:32]=selected_sequence;
            STAGE_SPEED:record[13*32 +:32]=saved_target;
            endcase
        end else if(operation==COMMIT_TARGET)record[11*32 +:32]=2;
        if(operation==TICK && saved_apply)begin
            record[11*32 +:32]=3;record[12*32 +:32]=selected_sequence;
            record[13*32 +:32]=saved_target;
        end
    end
    dmotor_shared_engine_transport transport(.ps_power_reset(ps_power_reset),.ps_valid(ps_valid),.ps_write(ps_write),.ps_address(ps_address),.ps_wdata(ps_wdata),.ps_strobes(ps_strobes),.ps_ready(ps_ready),.ps_response_valid(ps_response_valid),.ps_response_ready(ps_response_ready),.ps_response_okay(ps_response_okay),.ps_rdata(ps_rdata),.clk(clk),.reset(reset || fault),
        .command_valid(state==SEND && !fault),.command_ready(transport_ready),
        .command_record(record),.response_valid(response_valid),.response_ready(state==WAIT_REPLY),
        .response_words(reply),.response_error(response_error),.fault(transport_fault),
        .last_latency(ignored_latency),.maximum_latency(ignored_max));
    always @(posedge clk)begin
        if(reset)begin
            cleanup_busy<=0;cleanup_done<=0;cleanup_configurations<=0;
            state<=IDLE;operation<=RESET_CONTEXT;initialized<=0;stop_sent<=0;sequences<=0;active_targets<=0;ready_mask<=0;
            stopped<=0;done<=0;fault<=0;result_motor<=0;result_session<=0;result_generation<=0;
            result_tick<=0;result_duty<=0;selected_sequence<=0;saved_target<=0;
            saved_apply<=0;saved_stop<=0;saved_snapshot<=0;saved_config<=0;telemetry<=0;
        end else begin
            done<=0;cleanup_done<=0;
            for(integer i=0;i<4;i=i+1)if(!enabled[i])active_targets[i*32 +:32]<=0;
            stop_sent<=stop_sent & enabled;
            initialized<=initialized & enabled;ready_mask<=ready_mask & enabled;stopped<=stopped & enabled;
            // Fault/cancellation wins even over the last reset response.
            if(transport_fault || (cleanup_start && (state!=IDLE || start || enabled!=0 || session==0)) ||
               (cleanup_busy && (enabled!=0 || session!=result_session ||
                                  configurations!=cleanup_configurations || start)))begin
                fault<=1;cleanup_busy<=0;result_duty<=0;state<=IDLE;
            end else if(!fault || state==FINISH)begin
            case(state)
            IDLE:if(cleanup_start)begin
                cleanup_busy<=1;cleanup_configurations<=configurations;
                initialized<=0;ready_mask<=0;stopped<=0;stop_sent<=0;
                sequences<=0;active_targets<=0;telemetry<=0;
                result_motor<=0;result_session<=session;result_generation<=0;
                result_tick<=0;result_duty<=0;saved_snapshot<=0;
                saved_config<=configurations[0 +:640];saved_target<=0;
                saved_apply<=0;saved_stop<=0;selected_sequence<=0;
                operation<=RESET_CONTEXT;state<=SEND;
            end else if(start && ready)begin
                result_motor<=motor;result_session<=session;result_generation<=generation;result_tick<=tick;
                saved_snapshot<=snapshot;saved_config<=configurations[motor*640 +:640];
                saved_target<=target;saved_apply<=apply_target;saved_stop<=stop[motor];
                selected_sequence<=sequences[motor*32 +:32]+1'b1;
                if(generation!=configurations[motor*640 +:32] ||
                   (apply_target && (!ready_mask[motor] || stop[motor] || sequences[motor*32 +:32]==32'hffffffff)))begin
                    fault<=1;result_duty<=0;state<=FINISH;
                end else begin
                    operation<=!initialized[motor] ? RESET_CONTEXT : TICK;
                    state<=SEND;
                end
            end
            SEND:if(transport_ready)state<=WAIT_REPLY;
            WAIT_REPLY:if(response_valid)begin
                if(response_error || (cleanup_busy &&
                   (reply[415:0]!=0 || reply[13*32 +:32]!=result_session || reply[511:448]!=0)) || reply[8*32 +:32]!=0 || reply[9*32 +:32]!=0 ||
                   (operation>=STAGE_SESSION && operation<=COMMIT_TARGET && reply[31:0]!=1))begin
                    telemetry[result_motor*512 +:512]<=reply;
                    telemetry[result_motor*512+448 +:64]<=result_tick;
                    fault<=1;result_duty<=0;cleanup_busy<=0;state<=cleanup_busy ? IDLE : FINISH;
                end else if(cleanup_busy)begin
                    if(result_motor==3)begin
                        cleanup_busy<=0;cleanup_done<=1;state<=IDLE;
                    end else begin
                        result_motor<=result_motor+1'b1;
                        saved_config<=cleanup_configurations[(result_motor+1)*640 +:640];
                        state<=SEND;
                    end
                end else if(operation==RESET_CONTEXT)begin
                    initialized[result_motor]<=1;operation<=TICK;state<=SEND;
                end else if(operation==TICK)begin
                    if(saved_stop)stop_sent[result_motor]<=1;
                    telemetry[result_motor*512 +:512]<=reply;
                    telemetry[result_motor*512 +:32]<=saved_stop ? 0 : (saved_apply ? saved_target : active_targets[result_motor*32 +:32]);
                    telemetry[result_motor*512+448 +:64]<=result_tick;
                    if(saved_apply || saved_stop)active_targets[result_motor*32 +:32]<=saved_stop ? 0 : saved_target;
                    result_duty<=reply[4*32 +:32];
                    ready_mask[result_motor]<=reply[224] && !reply[226] && !reply[227] && !saved_stop;
                    stopped[result_motor]<=saved_stop && !reply[228] && reply[4*32 +:32]==0 && reply[5*32 +:32]==0;
                    if(saved_apply)begin
                        if(reply[2*32 +:32]!=selected_sequence || reply[1*32 +:32]!=0)begin
                            fault<=1;result_duty<=0;
                        end else sequences[result_motor*32 +:32]<=selected_sequence;
                    end
                    state<=FINISH;
                end else begin operation<=operation+1'b1;state<=SEND;end
            end
            FINISH:begin done<=1;state<=IDLE;end
            default:begin fault<=1;state<=IDLE;end
            endcase
            end
        end
    end
endmodule
