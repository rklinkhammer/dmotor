"""Real service/client processes with a test-only device, never physical MMIO."""
import os, socket, struct, subprocess, sys, tempfile, time
from pathlib import Path
with tempfile.TemporaryDirectory(prefix='dmotor-service-') as tmp:
    root=Path(tmp);env={**os.environ,'DMOTOR_TEST_DIRECTORY':tmp}
    config=root/'motors.conf'
    config.write_text('0 7 400000 1000000 2000000 1560 1 1 100 2 655 65536 32768 16384 1000000 2000000 1000000 8192\n')
    daemon=subprocess.Popen([sys.argv[1],'--externally-inhibited','/dev/fake',str(config)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    def connect():
        s=socket.socket(socket.AF_UNIX,socket.SOCK_SEQPACKET);s.settimeout(2);s.connect(str(root/'control.sock'));return s
    def request(s,op,mask=0,vel=(0,0,0,0)):
        s.send(struct.pack('<IIIiiii',1,op,mask,*vel));data=s.recv(192);assert len(data)==192
        return struct.unpack('<IIII',data[:16])
    try:
        for _ in range(200):
            if (root/'control.sock').exists(): break
            assert daemon.poll() is None
            time.sleep(.01)
        s=connect();assert request(s,0)[1]==1
        second=connect();assert second.recv(192)==b'';second.close()
        assert request(s,2)[1]==1
        assert request(s,3,1)[1]==1
        assert request(s,4,1,(1000,0,0,0))[1]==1
        time.sleep(.04);assert request(s,1)[3]>0
        # Inactivity expires ownership, closes client and leaves HALT latched.
        time.sleep(.6);assert s.recv(192)==b'';s.close()
        s=connect();assert request(s,1)[2]&(1<<18)
        assert request(s,3,1)[1]==0
        assert request(s,2)[1]==1
        assert request(s,3,1)[1]==1
        s.close();time.sleep(.03)
        s=connect();assert request(s,1)[2]&(1<<18)
        request(s,8);s.close();time.sleep(.03)
        # Interactive event loop renews while idle; ! interrupts partial input.
        menu=subprocess.Popen([sys.argv[2]],env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        menu.stdin.write('r\ne\n');menu.stdin.flush();time.sleep(.7)
        assert menu.poll() is None
        menu.stdin.write('v 12!\nq\n');menu.stdin.flush()
        out,err=menu.communicate(timeout=3)
        assert menu.returncode==0,(out,err)
        assert 'HALT confirmed' in out and 'HALT ALL confirmed' in out,(out,err)
        # EOF must also take the acknowledged HALT exit path.
        menu=subprocess.run([sys.argv[2]],env=env,input='',capture_output=True,text=True,timeout=3)
        assert menu.returncode==0,(menu.stdout,menu.stderr)
        assert 'HALT ALL confirmed' in menu.stdout
        # A killed client cannot run its exit handler: the service must inhibit.
        menu=subprocess.Popen([sys.argv[2]],env=env,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        menu.stdin.write('r\ne\n');menu.stdin.flush();time.sleep(.1)
        menu.kill();menu.communicate(timeout=3);time.sleep(.03)
        s=connect();assert request(s,1)[2]&(1<<18)
        assert request(s,3,1)[1]==0
        request(s,8);s.close()
        print('PASS: exclusive client, lease expiry, disconnect, stale enable rejection, idle menu lease, partial-input HALT, quit, EOF and killed client')
    finally:
        daemon.terminate()
        try: daemon.communicate(timeout=3)
        except subprocess.TimeoutExpired: daemon.kill();daemon.communicate()
