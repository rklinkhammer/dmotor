"""Rate-specific expectations in prepared test copies; production tests untouched."""
def adapt(out):
    p=out/'tests/motion_test.cpp';s=p.read_text().replace('double(integral) / 1000000.0','double(integral) / 200000.0').replace('reply[1] == 1000 && !reply[7]','reply[1] == 5000 && !reply[7]');p.write_text(s)
    p=out/'tests/controller_test.cpp';s=p.read_text().replace('i < 99','i < 19').replace('i < 28','i < 4').replace('i < 59','i < 11').replace('i < 60','i < 12');p.write_text(s)
    p=out/'tests/velocity_test.cpp';s=p.read_text().replace('tick <= 99','tick <= 19').replace('count = 101;','count = 501;').replace('99U, 100U}', '99U, 100U, 127U, 128U, 499U, 500U}');p.write_text(s)
    p=out/'tests/dispatcher_test.cpp';s=p.read_text().replace('i < 99','i < 19').replace('5, 1, 100, 2, 655','5, 1, 100, 10, 655');p.write_text(s)
