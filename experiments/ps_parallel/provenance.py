"""Include all PS executor inputs in the existing reproducible software archive."""
import json

def configure(out,period):
    p=out/'software/sources.json';names=set(json.loads(p.read_text()))
    for folder,glob in [('src','*.cpp'),('include','**/*.hpp')]:
        names.update(str(f.relative_to(out)) for f in (out/folder).glob(glob))
    names.update(['ps_parallel/engine.hpp','ps_parallel/ps_device.hpp','ps_parallel/ps_device.cpp','ps_profile.json'])
    p.write_text(json.dumps(sorted(names),indent=2)+'\n')
    p=out/'software/version.py';s=p.read_text();old="controller_abi='DMI2/DMB1-schema1'"
    assert s.count(old)==1
    p.write_text(s.replace(old,"controller_abi='DMI2/DMB1-schema1/PSF1-"+str(period)+"us'"))
