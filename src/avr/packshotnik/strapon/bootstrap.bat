rem this programs atmega8 for internal RC oscillator @ 8MHz -- calibration may be required
rem avrdude -p m8 -c pony-stk200 -P lpt1 -U hfuse:w:0xdc:m -U lfuse:w:0xe4:m
avrdude -p m8 -c pony-stk200 -P lpt1 -U flash:w:"boot_atmega8.hex":i -v -V -u
