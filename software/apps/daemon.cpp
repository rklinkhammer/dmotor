#include "dmotor_sw/kernel_device.hpp"
#include "dmotor_sw/service_protocol.hpp"
#include "dmotor_sw/version.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <poll.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cerrno>
#include <cstdlib>
using namespace dmotor_sw;
namespace {
volatile std::sig_atomic_t exiting=0;
void signal_exit(int) { exiting=1; }
#ifdef DMOTOR_TEST_SERVICE
const std::string service_directory=std::getenv("DMOTOR_TEST_DIRECTORY");
#else
const std::string service_directory="/run/dmotor";
#endif
const std::string socket_name=service_directory+"/control.sock";
const char* socket_path=socket_name.c_str();
struct Socket {
    int fd=-1;
    ~Socket() { if(fd>=0) close(fd); }
};
uint32_t load(const char* path,std::array<MotorConfiguration,4>& configs) {
    std::ifstream in(path);if(!in) throw IoError("Cannot open configuration");
    uint32_t mask=0;std::string line;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#') continue;
        // Every configuration field is nonnegative; stream extraction into an
        // unsigned field otherwise accepts negative text modulo 2^32.
        if(line.find('-')!=std::string::npos) throw IoError("Negative configuration value");
        unsigned m=4; MotorConfiguration c;std::istringstream s(line);
        if(!(s>>m) || m>=4 || (mask&(1U<<m))) throw IoError("Invalid/duplicate motor row");
        if(!(s>>c.generation>>c.maximum_mrpm>>c.acceleration_mrpm_s>>c.deceleration_mrpm_s
             >>c.counts_numerator>>c.counts_denominator>>c.position_tolerance_counts>>c.velocity_tolerance_mrpm
             >>c.settle_ticks>>c.kp_q16>>c.ki_q16>>c.duty_limit_q16>>c.integral_limit_q16>>c.stop_timeout_us
             >>c.zero_timeout_cycles>>c.acquisition_timeout_cycles>>c.acquisition_duty_limit_q16)) throw IoError("Incomplete configuration row");
        std::string extra;if(s>>extra || !valid_configuration(c)) throw IoError("Invalid configuration values");
        configs[m]=c;mask|=1U<<m;
    }
    if(!mask) throw IoError("No configured motors");
    return mask;
}
}
int main(int argc,char** argv) {
    if(startup_version("dmotord",argc,argv)) return 0;
    const bool direct=argc==7 && std::string(argv[4])=="--direct-duty";
    const bool commissioning=direct || (argc==5 && std::string(argv[4])=="--commissioning");
    if((argc!=4 && !commissioning) || std::string(argv[1])!="--externally-inhibited") {
        std::cerr<<"Usage: dmotord --externally-inhibited /dev/dmotor0 CONFIG [--commissioning | --direct-duty MAX_PERCENT FLOOR_PERCENT]\nInitial commissioning only; disconnect drive power.\n";return 2;
    }
    try {
        std::array<MotorConfiguration,4> configs{};const auto mask=load(argv[3],configs);
        KernelDevice device(argv[2]);
        RuntimePolicy policy;policy.commissioning=commissioning;
        if(direct) {
            auto percent=[](const char* text) {
                std::string s(text);if(s.empty() || s.find_first_not_of("0123456789")!=std::string::npos)throw IoError("Duty limits must be whole percentages 0..100");
                const auto n=std::stoul(s);if(n>100)throw IoError("Duty exceeds 100 percent");
                return static_cast<uint32_t>((n*65536+50)/100);
            };
            policy.direct_mask=mask;policy.duty_limit_q16=percent(argv[5]);policy.duty_floor_q16=percent(argv[6]);
            std::cout<<"DIRECT DUTY: mask="<<mask<<" limit_q16="<<policy.duty_limit_q16<<" floor_q16="<<policy.duty_floor_q16<<"; no velocity regulation\n";
        }
        Runtime runtime(device,policy);
        if(mkdir(service_directory.c_str(),0700) && errno!=EEXIST) throw IoError("Cannot create service directory");
        struct stat st{};if(lstat(service_directory.c_str(),&st) || !S_ISDIR(st.st_mode) || st.st_uid!=geteuid() || (st.st_mode&077)) throw IoError("Unsafe service directory");
        Socket server;server.fd=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
        if(server.fd<0) throw IoError("Cannot create socket");
        // Exclusive device open above prevents removing a live owner's socket.
        if(unlink(socket_path) && errno!=ENOENT) throw IoError("Cannot remove stale socket");
        sockaddr_un address{};address.sun_family=AF_UNIX;std::strcpy(address.sun_path,socket_path);
        if(bind(server.fd,reinterpret_cast<sockaddr*>(&address),sizeof(address)) || chmod(socket_path,0600) || listen(server.fd,2)) throw IoError("Cannot bind service socket");
        if(std::signal(SIGINT,signal_exit)==SIG_ERR || std::signal(SIGTERM,signal_exit)==SIG_ERR) throw IoError("Cannot install signal handlers");
        Socket client;
        uint64_t last_client=0;
        // CSV is diagnostic, monotonic host time; scope edges establish electrical latency.
        auto trace=[&](const char* event) {
            if(!commissioning) return;
            const auto s=runtime.status();const auto t=runtime.capture();
            for(unsigned m=0;m<4;++m) {
                const auto& v=t.motors[m];
                std::cout<<device.now_us()<<','<<event<<','<<runtime.session()<<','<<s.raw<<','<<m
                         <<','<<v.position_counts<<','<<v.measured_mrpm<<','<<v.duty_q16
                         <<','<<v.engine_faults<<','<<v.protection_faults<<'\n';
            }
            std::cout.flush();
        };
        if(commissioning) std::cout<<"time_us,event,session,status,motor,position,measured_mrpm,duty_q16,engine_faults,protection_faults\n";
        uint64_t next_trace=0;
        constexpr uint64_t trace_period_us=20000;
        auto disconnect=[&]() { runtime.halt();if(client.fd>=0) { close(client.fd);client.fd=-1; } };
        std::cout<<"Inhibited commissioning service ready; no automatic rearm or enable.\n";
        uint64_t next_tick=device.now_us();
        constexpr uint64_t tick_us=service_period_ms*1000;
        while(!exiting) {
            pollfd fds[2]={{server.fd,POLLIN,0},{client.fd,POLLIN,0}};
            const auto before_poll=device.now_us();
            const int timeout=before_poll>=next_tick?0:static_cast<int>((next_tick-before_poll+999)/1000);
            const int polled=poll(fds,2,timeout);
            if(exiting) break;
            if(polled<0 && errno!=EINTR) throw IoError("poll failed");
            if(fds[0].revents&POLLIN) {
                int fd=accept4(server.fd,nullptr,nullptr,SOCK_CLOEXEC|SOCK_NONBLOCK);
                if(fd>=0) {
                    if(client.fd>=0) close(fd);
                    else { client.fd=fd;last_client=device.now_us();runtime.heartbeat(); }
                }
            }
            if(client.fd>=0 && device.now_us()-last_client>=policy.client_lease_us) disconnect();
            if(client.fd>=0 && (fds[1].revents&(POLLHUP|POLLERR|POLLNVAL))) disconnect();
            if(client.fd>=0 && (fds[1].revents&POLLIN)) {
                Request request{};Response response{};
                const auto n=recv(client.fd,&request,sizeof(request),MSG_TRUNC);
                if(n!=sizeof(request) || request.schema!=service_schema) { disconnect();continue; }
                last_client=device.now_us();runtime.heartbeat();
                bool quit=false;
                try {
                    std::array<int32_t,4> velocities{};
                    for(unsigned i=0;i<4;++i) velocities[i]=request.velocity[i];
                    switch(request.operation) {
                    case Operation::duty:runtime.duty(request.velocity[0]);break;
                    case Operation::inject_target:trace("inject_target");runtime.inject_timeout(TimeoutInjection::target);break;
                    case Operation::inject_watchdog:trace("inject_watchdog");runtime.inject_timeout(TimeoutInjection::watchdog);break;
                    case Operation::ping: case Operation::status: break;
                    case Operation::prepare: runtime.prepare(configs,mask);break;
                    case Operation::enable:runtime.enable(request.mask);break;
                    case Operation::velocity:runtime.velocity(request.mask,velocities);break;
                    case Operation::stop:runtime.stop(request.mask);break;
                    case Operation::disable:runtime.disable(request.mask);break;
                    case Operation::halt:runtime.halt();break;
                    case Operation::quit:runtime.halt();quit=true;break;
                    default:throw IoError("Unknown operation");
                    }
                    const auto s=runtime.status();response.status=s.raw;response.applied=runtime.applied_sequence();
                    if(request.operation==Operation::status) {
                        const auto t=runtime.capture();
                        for(unsigned m=0;m<4;++m) {
                            response.measured[m]=t.motors[m].measured_mrpm;
                            const auto p=static_cast<uint64_t>(t.motors[m].position_counts);
                            response.position_lo[m]=static_cast<uint32_t>(p);response.position_hi[m]=static_cast<uint32_t>(p>>32);
                        }
                    }
                    if(request.operation!=Operation::ping) trace("command");
                    response.okay=1;
                } catch(const std::exception& e) {
                    runtime.halt();std::strncpy(response.message,e.what(),sizeof(response.message)-1);
                }
                if(send(client.fd,&response,sizeof(response),MSG_NOSIGNAL)!=sizeof(response)) { disconnect();continue; }
                if(quit) disconnect();
            }
            if(device.now_us()>=next_tick) {
                try {
                    // Snapshot before tick can assert host HALT, preserving original fault evidence.
                    if(commissioning) {
                        const auto s=runtime.status();
                        if(device.now_us()>=next_trace || s.fault || s.shared_halt) {
                            trace("sample");next_trace=device.now_us()+trace_period_us;
                        }
                    }
                    runtime.tick();
                }
                catch(const std::exception& e) { std::cerr<<device.now_us()<<" HALT: "<<e.what()<<'\n';disconnect(); }
                // Absolute monotonic schedule; skip missed ticks rather than replaying.
                const auto now=device.now_us();
                next_tick+=((now-next_tick)/tick_us+1)*tick_us;
            }
        }
        runtime.halt();unlink(socket_path);
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
