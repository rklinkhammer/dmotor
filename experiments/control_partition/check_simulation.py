#!/usr/bin/env python3
"""Numerical and scenario checks, not motor or target-platform qualification."""
import json
from pathlib import Path
import simulate_ps as sim

checks={'fabric_contract':sim.check_contract()}
a=sim.run(200,.1)
sim.STEP=.00005
b=sim.run(200,.1)
checks['step_100us_vs_50us']={
    'rms_difference_m_s':abs(a['wheel_tracking_rms_m_s']-b['wheel_tracking_rms_m_s']),
    'pose_max_component_difference':max(abs(x-y) for x,y in zip(a['pose'],b['pose']))}
assert checks['step_100us_vs_50us']['rms_difference_m_s']<.001
assert checks['step_100us_vs_50us']['pose_max_component_difference']<.001
sim.STEP=.0001
runs=[sim.run(200,tau,'jitter',feedback,seed)
      for tau in [.02,.1,.3] for feedback in ['ideal','counts'] for seed in [2,3,4]]
assert all(r['inhibited_at_s'] is None for r in runs)
assert all(r['max_sample_interval_ms']<=7.1 for r in runs)
checks['additional_200hz_jitter_runs']=runs
checks['speed_sensitivity_runs']=[
    sim.run(hz,tau,scenario,'counts',1,scale)
    for hz in [1000,200] for tau in [.02,.1,.3]
    for scenario in ['nominal','jitter'] for scale in [1/.6,2/.6]]
assert all(r['inhibited_at_s'] is None for r in checks['speed_sensitivity_runs'])
assert len(checks['speed_sensitivity_runs']) == 24
# Kinematic transform inversion, with signs and combined translation/rotation.
for twist in [(1,0,0),(0,-1,0),(0,0,1),(.3,-.2,.7)]:
 assert all(abs(a-b)<1e-12 for a,b in zip(twist,sim.body(sim.wheels(*twist))))
checks['kinematic_round_trip']='PASS'
out=Path(__file__).resolve().parents[2]/'build/partition/simulation_checks.json'
out.write_text(json.dumps(checks,indent=2)+'\n')
print('PASS: event/contract checks, timestep refinement, 18 additional jitter runs, '
      '24 speed sensitivity runs, kinematics')
