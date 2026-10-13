// Serialized adaptation to the measured C++ HLS engine. No motor arithmetic here.
module dmotor_motor_engine_adapter(
    input wire clk, reset,
    input wire start,
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
    wire transport_ready,response_valid,response_error,transport_fault;
    wire [511:0] reply;
    wire [31:0] ignored_latency,ignored_max;
    assign ready=state==IDLE && !reset && !fault;
    always @* begin
        record=0;
        record[31:0]={30'd0,result_motor};
        record[32]=operation==RESET_CONTEXT;
        record[33]=operation==TICK;
        record[34]=1;
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
    end
    dmotor_shared_engine_transport transport(.clk(clk),.reset(reset),
        .command_valid(state==SEND && !fault),.command_ready(transport_ready),
        .command_record(record),.response_valid(response_valid),.response_ready(state==WAIT_REPLY),
        .response_words(reply),.response_error(response_error),.fault(transport_fault),
        .last_latency(ignored_latency),.maximum_latency(ignored_max));
    always @(posedge clk)begin
        if(reset)begin
            state<=IDLE;operation<=RESET_CONTEXT;initialized<=0;stop_sent<=0;sequences<=0;active_targets<=0;ready_mask<=0;
            stopped<=0;done<=0;fault<=0;result_motor<=0;result_session<=0;result_generation<=0;
            result_tick<=0;result_duty<=0;selected_sequence<=0;saved_target<=0;
            saved_apply<=0;saved_stop<=0;saved_snapshot<=0;saved_config<=0;telemetry<=0;
        end else begin
            done<=0;
            for(integer i=0;i<4;i=i+1)if(!enabled[i])active_targets[i*32 +:32]<=0;
            stop_sent<=stop_sent & enabled;
            initialized<=initialized & enabled;ready_mask<=ready_mask & enabled;stopped<=stopped & enabled;
            if(transport_fault)fault<=1;
            case(state)
            IDLE:if(start && ready)begin
                result_motor<=motor;result_session<=session;result_generation<=generation;result_tick<=tick;
                saved_snapshot<=snapshot;saved_config<=configurations[motor*640 +:640];
                saved_target<=target;saved_apply<=apply_target;saved_stop<=stop[motor];
                selected_sequence<=sequences[motor*32 +:32]+1'b1;
                if(generation!=configurations[motor*640 +:32] ||
                   (apply_target && (!ready_mask[motor] || stop[motor] || sequences[motor*32 +:32]==32'hffffffff)))begin
                    fault<=1;result_duty<=0;state<=FINISH;
                end else begin
                    operation<=!initialized[motor] ? RESET_CONTEXT : (apply_target ? STAGE_SESSION : TICK);
                    state<=SEND;
                end
            end
            SEND:if(transport_ready)state<=WAIT_REPLY;
            WAIT_REPLY:if(response_valid)begin
                if(response_error || reply[8*32 +:32]!=0 || reply[9*32 +:32]!=0 ||
                   (operation>=STAGE_SESSION && operation<=COMMIT_TARGET && reply[31:0]!=1))begin
                    telemetry[result_motor*512 +:512]<=reply;
                    telemetry[result_motor*512+448 +:64]<=result_tick;
                    fault<=1;result_duty<=0;state<=FINISH;
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
endmodule
