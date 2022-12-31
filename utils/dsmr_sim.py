#!/usr/bin/python3

import time

import serial

PORT = "/dev/serial/by-id/usb-Silicon_Labs_CP2104_USB_to_UART_Bridge_Controller_011F15F2-if00-port0"
BAUD_RATE = 115200
INTERVAL = 1
TELEGRAM = '''
/Ene5\T210-D ESMR5.0

1-3:0.2.8(50)
0-0:1.0.0(180108202537W)
0-0:96.1.1(serienummer)
1-0:1.8.1(000112.286*kWh)
1-0:1.8.2(000000.000*kWh)
1-0:2.8.1(000000.084*kWh)
1-0:2.8.2(000000.000*kWh)
0-0:96.14.0(0002)
1-0:1.7.0(00.134*kW)
1-0:2.7.0(00.000*kW)
0-0:96.7.21(00008)
0-0:96.7.9(00004)
1-0:99.97.0(1)(0-0:96.7.19)(171024204625S)(0000000305*s)
1-0:32.32.0(00003)
1-0:52.32.0(00003)
1-0:72.32.0(00002)
1-0:32.36.0(00000)
1-0:52.36.0(00000)
1-0:72.36.0(00000)
0-0:96.13.0()
1-0:32.7.0(229.1*V)
1-0:52.7.0(229.1*V)
1-0:72.7.0(229.1*V)
1-0:31.7.0(%03d*A)
1-0:51.7.0(%03d*A)
1-0:71.7.0(%03d*A)
1-0:21.7.0(01.150*kW)
1-0:41.7.0(01.150*kW)
1-0:61.7.0(01.150*kW)
1-0:22.7.0(00.000*kW)
1-0:42.7.0(00.000*kW)
1-0:62.7.0(00.000*kW)
0-1:24.1.0(003)
0-1:96.1.0(serienummer)
0-1:24.2.1(180108205500W)(00001.290*m3)
!B055
'''

with serial.Serial(PORT, BAUD_RATE) as ser:
    i = 0
    current = 5
    while True:
        i = i + 1
        print("Sending telegram", i, end='\r')
        t = TELEGRAM % (current, current, current)
        for line in t.splitlines():
            ser.write(line.strip().encode('US-ASCII') + b"\r\n")
        time.sleep(INTERVAL)
