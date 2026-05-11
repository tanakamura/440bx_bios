00-0f : pre car
  00  : boot
  01  : uart init done
  0E  : before car
10-1f : C
  10  : car done
  11  : enter to C
  12  : leave CAR / switch to DRAM stack
  13  : running on DRAM stack
  14  : entered BIOS payload

E0-EF : tmp
  E7  : pci probe start
  E8  : smbus probe start
  E9  : dram init start
  EA  : dram test pass
F0-FF : fatal
  F0  : unsupported dram configuration
  F1  : dram init failed
  F2  : dram test failed
  FF  : bist fail
