#!/usr/bin/env python3
"""Apply the current capability contract after historical exact-profile patches."""
from pathlib import Path

def apply(out):
    def edit(name, old, new):
        path = out / name
        text = path.read_text()
        if text.count(old) != 1:
            raise RuntimeError(f'{name}: expected one capability edit for {old!r}')
        path.write_text(text.replace(old, new))
    edit('include/dmotor/operating_limits.hpp', '#include <cstdint>',
         '#include <cstdint>\n#include "dmotor/capabilities.hpp"')
    edit('include/dmotor/operating_limits.hpp', 'max_shaft_speed_rpm = 200',
         'max_shaft_speed_rpm = capability::maximum_speed_rpm')
    edit('include/dmotor/operating_limits.hpp', 'velocity_bits = 19',
         'velocity_bits = capability::velocity_bits')
    edit('src/motion.cpp', 'narrow_signed<19>', 'narrow_signed<operating::velocity_bits>')
    edit('include/dmotor/motion.hpp', 'Validated +/-200000 milliRPM.',
         'Validated against capability::maximum_speed_mrpm.')
    arithmetic = out / 'src/arithmetic.cpp'
    if 'divide_pi(' in arithmetic.read_text():
        edit('src/arithmetic.cpp', '#include "dmotor/detail/arithmetic.hpp"',
             '#include "dmotor/detail/arithmetic.hpp"\n#include "dmotor/capabilities.hpp"')
        edit('src/arithmetic.cpp', 'numerator_bits = 39', 'numerator_bits = capability::pi_numerator_bits')
        edit('src/arithmetic.cpp', 'denominator_bits = 20', 'denominator_bits = capability::pi_denominator_bits')
        edit('include/dmotor/detail/arithmetic.hpp', 'numerator < 2^39', 'numerator < 2^40')
        edit('src/motion.cpp', '''// Bounded exact milliRPM: |error| <= 400000, gain <= 1048576.
    // P rounded numerator <= 419430400500; clamped I <= 65536500000.
    // Divisors are 1000 and 1000000. Both calls fit the shared 39/20 core.''',
             '''// Bounded exact milliRPM: |error| <= 800000, gain <= 1048576.
    // P rounded numerator <= 838860800500; clamped I <= 65536500000.
    // Divisors are 1000 and 1000000. Both calls fit the shared 40/20 core.''')
        edit('tests/arithmetic_test.cpp', 'uint64_t{1} << 39', 'uint64_t{1} << 40')
        edit('tests/arithmetic_test.cpp', 'uint64_t{419430400500}', 'uint64_t{838860800500}')
    for name in ('tests/motion_test.cpp', 'tests/velocity_test.cpp'):
        path = out / name
        text = path.read_text().replace('200001', '400001').replace('200000', '400000')
        path.write_text(text)

    path = out / 'tests/motion_test.cpp'
    text = path.read_text()
    marker = '    // Exhaust every accepted milliRPM value against an independent FP oracle.'
    cases = """    // Opposite-sign feedback exercises the maximum PI product/divider input.
    for (int sign : {-1, 1}) {
        config[1] = dmotor::capability::maximum_speed_mrpm;
        config[2] = 10000000; config[3] = 10000000;
        config[9] = 1048576; config[10] = 1048576;
        config[11] = 65536; config[12] = 65536;
        reset(); command[0] = 1;
        command[5] = static_cast<uint32_t>(sign * dmotor::capability::maximum_speed_mrpm);
        for (unsigned i=0; i<85; ++i) {
            tick(i==0, 0, -sign * dmotor::capability::maximum_speed_mrpm);
            CHECK(!reply[7]);
            CHECK(dmotor::signed_word(reply[0]) == sign * 65536);
        }
        // Non-saturating independently computed output at the same error.
        config[9] = 1; config[10] = 0;
        reset(); command[0] = 1;
        command[5] = static_cast<uint32_t>(sign * dmotor::capability::maximum_speed_mrpm);
        for(unsigned i=0;i<85;++i)
            tick(i==0, 0, -sign * dmotor::capability::maximum_speed_mrpm);
        CHECK(!reply[7] && dmotor::signed_word(reply[0]) == sign * 800);
    }
    config[2]=1000000; config[3]=1000000; config[9]=655; config[10]=65536;
"""
    if text.count(marker) != 1:
        raise RuntimeError('Motion boundary test hook changed')
    path.write_text(text.replace(marker, cases + marker))
