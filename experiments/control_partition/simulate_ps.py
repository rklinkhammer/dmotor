#!/usr/bin/env python3
"""Offline PS-control feasibility experiment, NOT production control or protection.
Fictional first-order wheel plants; no electrical/current model, calibrated slip,
Linux timing measurements, or actual daemon/driver/FPGA connection. PI uses actual
sample intervals. Four duties are applied atomically by a proposed fabric model.
The production estimator, exact integer arithmetic and acquisition logic are NOT
ported here. Ideal feedback isolates timing; count feedback probes quantization.
"""
import argparse
import heapq
import json
import math
import random
from dataclasses import dataclass
from pathlib import Path

STEP = 0.0001  # integration and event-grid resolution, 100 us
RADIUS = 0.05
ARM = 0.30  # half wheelbase + half track
MAX_SPEED = 2.0
EXPIRY = 0.020  # proposed maximum age from encoder capture, not receipt


def wheels(vx, vy, yaw):
    return [vx-vy-ARM*yaw, vx+vy+ARM*yaw,
            vx+vy-ARM*yaw, vx-vy+ARM*yaw]


def body(w):
    return ((w[0]+w[1]+w[2]+w[3])/4,
            (-w[0]+w[1]+w[2]-w[3])/4,
            (-w[0]+w[1]-w[2]+w[3])/(4*ARM))


def request(t):
    if t < .3: return [0.0]*4
    if t < 1.8: return wheels(.6, 0, 0)
    if t < 3.0: return wheels(0, .4, .3)
    if t < 4.5: return wheels(-.4, .2, -.2)
    return [0.0]*4  # controlled stop, with ongoing publications


@dataclass
class Frame:
    session: int
    sequence: int
    captured: float
    duty: tuple


class Fabric:
    """Proposed contract; independent stale-duty latch, expiry wins at equality.
    No automatic rearm. Session 1 has been explicitly armed at t=0 in this model.
    Delayed frames cannot renew an expired sample, replay or clear inhibition.
    """
    def __init__(self, expiry=EXPIRY):
        self.expiry = expiry
        self.session = 1
        self.sequence = -1
        self.captured = 0.0
        self.duty = (0.0,)*4
        self.inhibited = False
        self.reason = None
        self.rejected = 0
        self.captures = {}

    def capture(self, now, token):
        """Fabric-issued snapshot identity; this call does NOT renew duty expiry."""
        self.captures = {k:v for k,v in self.captures.items() if now-v < self.expiry}
        self.captures[token] = now

    def tick(self, now, external_fault=False):
        if external_fault or now-self.captured >= self.expiry-1e-12:
            if not self.inhibited:
                self.reason = 'external_fault' if external_fault else 'stale_duty'
            self.inhibited = True
        if self.inhibited: self.duty = (0.0,)*4

    def accept(self, now, f):
        self.tick(now)
        if (self.inhibited or f.session != self.session or f.sequence <= self.sequence
                or self.captures.get(f.sequence) != f.captured
                or f.captured > now+1e-12 or f.captured < self.captured
                or now-f.captured >= self.expiry-1e-12
                or len(f.duty) != 4 or any(not math.isfinite(x) or abs(x)>1 for x in f.duty)):
            self.rejected += 1
            return False
        self.sequence, self.captured, self.duty = f.sequence, f.captured, f.duty
        return True


def run(hz, tau, scenario='nominal', feedback='ideal', seed=1, speed_scale=1.0):
    rng = random.Random(seed)
    fabric = Fabric()
    speed = [0.0]*4
    distance = [0.0]*4
    ramp = [0.0]*4
    integral = [0.0]*4
    previous_counts = [0]*4
    previous_sample = 0.0
    pose = [0.0]*3
    next_release = 0.0
    queue = []
    sequence = 0
    release_index = 0
    first_inhibit = None
    sample_intervals = []
    ages = []
    squared_error = 0.0
    samples = 0
    max_error = 0.0
    late_intervals = 0
    end_speed = None
    distance_after_inhibit = 0.0
    stretch = max(1.0, speed_scale)  # allow faster requests to finish accelerating
    maximum_body_speed = 0.0
    for k in range(round(6.0*stretch/STEP)+1):
        now = k*STEP
        external = scenario == 'fault' and now >= 2.5
        fabric.tick(now, external)
        if now+1e-12 >= next_release:
            release_index += 1
            # Jitter is release-time variation, not an assumed CPU execution bound.
            jitter = rng.uniform(-.001, .001) if scenario == 'jitter' else 0.0
            # Minimum 100 us spacing; the 1 kHz jitter stress can bunch releases.
            next_release = max(now+STEP, release_index/hz+jitter)
            stalled = scenario == 'stall' and 2.5 <= now < 2.6
            if not stalled and not fabric.inhibited:
                fabric.capture(now, sequence)
                dt = now-previous_sample if sequence else 1/hz
                sample_intervals.append(dt)
                if dt > 2/hz: late_intervals += 1
                counts = [math.floor(x/(2*math.pi*RADIUS)*2048) for x in distance]
                measured = speed[:]
                if feedback == 'counts':
                    measured = [(counts[i]-previous_counts[i])*(2*math.pi*RADIUS/2048)/dt
                                for i in range(4)]
                previous_counts, previous_sample = counts, now
                duty = []
                targets = [x*speed_scale for x in request(now/stretch)]
                ratio = max(1.0, max(abs(x) for x in targets)/MAX_SPEED)
                targets = [x/ratio for x in targets]
                for i in range(4):
                    goal = targets[i]
                    if goal*ramp[i] < 0: goal = 0.0
                    rate = 1.5 if abs(goal)<abs(ramp[i]) else 1.0
                    ramp[i] += max(-rate*dt, min(rate*dt, goal-ramp[i]))
                    error = ramp[i]-measured[i]
                    candidate = max(-.5, min(.5, integral[i]+2.0*error*dt))
                    effort = ramp[i]/MAX_SPEED+.5*error+candidate
                    if abs(effort)<=1 or effort*error < 0: integral[i]=candidate
                    duty.append(max(-1.0,min(1.0,ramp[i]/MAX_SPEED+.5*error+integral[i])))
                latency = {'delay8ms':.008, 'delay25ms':.025}.get(scenario,.0005)
                heapq.heappush(queue,(now+latency,sequence,Frame(1,sequence,now,tuple(duty))))
                sequence += 1
        while queue and queue[0][0] <= now+1e-12:
            _, _, frame = heapq.heappop(queue)
            fabric.accept(now,frame)
        if fabric.inhibited and first_inhibit is None: first_inhibit=now
        if not fabric.inhibited: ages.append(now-fabric.captured)
        for i in range(4):
            # Neutral/inhibited output coasts with a separate assumed 400 ms constant.
            tc = .4 if fabric.inhibited else tau*(.85+.1*i)
            # Opposing load disturbance; must not drive a stationary wheel.
            load = (math.copysign(.08, speed[i])
                    if 3.3*stretch <= now < 3.8*stretch and abs(speed[i]) > .01 else 0.0)
            derivative=(MAX_SPEED*fabric.duty[i]-speed[i]-load)/tc
            derivative=max(-4.0,min(4.0,derivative))
            old=speed[i]
            speed[i] += derivative*STEP
            distance[i] += (old+speed[i])/2*STEP
            if not fabric.inhibited and now>.3:
                error=speed[i]-ramp[i]
                squared_error += error*error
                samples += 1
                max_error=max(max_error,abs(error))
        vx,vy,yaw=body(speed)
        maximum_body_speed = max(maximum_body_speed, math.hypot(vx,vy))
        if fabric.inhibited: distance_after_inhibit += math.hypot(vx,vy)*STEP
        heading=pose[2]+yaw*STEP/2
        pose[0] += (math.cos(heading)*vx-math.sin(heading)*vy)*STEP
        pose[1] += (math.sin(heading)*vx+math.cos(heading)*vy)*STEP
        pose[2] += yaw*STEP
        if k==round((4.5*stretch+1.0)/STEP): end_speed=max(abs(x) for x in speed)
    return dict(hz=hz,tau_s=tau,scenario=scenario,feedback=feedback,seed=seed,
                speed_scale=speed_scale,
                duration_s=6.0*stretch,max_body_speed_m_s=maximum_body_speed,
                wheel_tracking_rms_m_s=math.sqrt(squared_error/max(samples,1)),
                wheel_tracking_max_m_s=max_error,pose=pose,
                max_sample_interval_ms=max(sample_intervals)*1000,
                max_applied_sample_age_ms=max(ages,default=0)*1000,
                inhibited_at_s=first_inhibit,inhibit_reason=fabric.reason,
                rejected_frames=fabric.rejected,intervals_above_two_periods=late_intervals,
                distance_after_inhibit_m=distance_after_inhibit,
                max_wheel_speed_1s_after_stop_m_s=end_speed)


def check_contract():
    f=Fabric()
    f.capture(0,1)
    assert f.accept(.001,Frame(1,1,0,(.1,.2,.3,.4)))
    old=f.duty
    f.capture(.001,2)
    assert not f.accept(.002,Frame(1,1,.001,(0,)*4)) and f.duty==old
    assert not f.accept(.002,Frame(2,2,.001,(0,)*4)) and f.duty==old
    assert not f.accept(.002,Frame(1,2,.003,(0,)*4))
    assert not f.accept(.002,Frame(1,2,.001,(0,0,0,float('nan'))))
    f.capture(EXPIRY,2)
    assert not f.accept(EXPIRY,Frame(1,2,EXPIRY,(.5,)*4))
    assert f.inhibited and f.duty==(0,)*4  # equality expiry beats a fresh frame
    assert not f.accept(.021,Frame(1,3,.021,(.5,)*4))  # no automatic restart
    g=Fabric(); g.capture(.001,2)
    assert g.accept(.001,Frame(1,2,.001,(.5,)*4))
    assert not g.accept(.002,Frame(1,3,.002,(.6,)*4))  # forged capture identity
    assert not g.accept(.002,Frame(1,3,0,(.6,)*4))  # unrecognized older capture
    g.tick(.002,True); assert g.inhibited and g.reason=='external_fault'
    assert not g.accept(.003,Frame(1,4,.003,(.6,)*4))
    return 'PASS: atomic frames, replay/session/sample checks, expiry priority, sticky inhibit'


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    result={'contract_tests':check_contract(),'assumptions':{
        'fictional_plant':True,'integration_step_s':STEP,'wheel_radius_m':RADIUS,
        'L_plus_W_m':ARM,'counts_per_wheel_revolution':2048,
        'proposed_duty_expiry_s':EXPIRY,'current_production_contract':False},'runs':[]}
    for tau in [.02,.1,.3]:
        for hz in [1000,200,100]:
            for scenario in ['nominal','jitter','delay8ms','delay25ms','stall','fault']:
                for feedback in ['ideal','counts']:
                    row=run(hz,tau,scenario,feedback)
                    if scenario=='stall':
                        assert row['inhibit_reason']=='stale_duty'
                        assert 2.5<=row['inhibited_at_s']<=2.521
                    elif scenario=='delay25ms':
                        assert row['inhibit_reason']=='stale_duty'
                        assert abs(row['inhibited_at_s']-.020)<1e-12
                    elif scenario=='fault':assert row['inhibited_at_s']==2.5
                    else:assert row['inhibited_at_s'] is None,row
                    result['runs'].append(row)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(result['contract_tests'])
    print('PASS:',len(result['runs']),'fictional four-wheel plant scenarios; results:',a.output)
