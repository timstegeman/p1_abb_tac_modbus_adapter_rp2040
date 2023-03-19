# P1 Modbus load balancer

[//]: # (This firmware connects to a P1 smart meter and stores the necesaary values in modbus registers needed for the load)

[//]: # (balancing functionality of the ABB Terra AC EV charger.)

[//]: # ()

[//]: # (This is only tested with a Sagemcom XS210 smart meter, but could easily be adapted for other ESMR/DSMR formats.)

[//]: # ()

[//]: # (P1 output will be also be available on the USB port of the Raspberrt Pi Pico.)

### Modbus registers

| Register | R/W | Description                                                 | Resolution | Unit   | Default |
|----------|-----|-------------------------------------------------------------|------------|--------|---------|
| 1000     | RW  | Override the current load balancer limit (applied directly) | 0.001      | A      | 16 A    |
| 1001     | R   | The current limit decided by the load balancer              | 0.001      | A      |         |
| 1002     | R   | The current load balancer state                             |            |        |         |
| 1010     | RW  | The maximum charger current of the charger                  | 0.001      | A      | 16 A    |
| 1011     | RW  | Number of phases                                            |            |        | 3       |
| 1012     | RW  | Alarm limit current                                         | 0.001      | A      | 24 A    |
| 1013     | RW  | Alarm limit wait time                                       | 1          | second | 1 s     |
| 1014     | RW  | Alarm limit current change amount                           | 0.001      | A      | 12.5A   |
| 1015     | RW  | Upper limit current                                         | 0.001      | A      | 22 A    |
| 1016     | RW  | Upper limit wait time                                       | 1          | second | 5 s     |
| 1017     | RW  | Upper limit current change amount                           | 0.001      | A      | 1 A     |
| 1018     | RW  | Lower limit current                                         | 0.001      | A      | 19 A    |
| 1019     | RW  | Lower limit wait time                                       | 1          | second | 1 s     |
| 1020     | RW  | Lower limit current change amount                           | 0.001      | A      | 1 A     |
| 1021     | RW  | Fallback limit                                              | 0.001      | A      | 0 A     |
| 1022     | RW  | Fallback limit time                                         | 1          | second | 30 s    |
| 1090     | W   | Change the modbus server address                            |            |        | 10      |
| 1091     | W   | Save and apply configuration (write 1)                      |            |        |         |
| 1092     | W   | Restore defaults (write 1)                                  |            |        |         |

The defaults are bases on an 11 kW charger on an 3 phase 25 A grid connection.

#### Load balancer state

| State | Description    | LED Indication                       |
|-------|----------------|--------------------------------------|
| 0     | Normal         | `._______` (1 pulse per second)      |
| 1     | Lower limit    | `._._____` (2 pulses per second)     |
| 2     | Upper limit    | `._._.___` (3 pulses per second)     |
| 3     | Alarm limit    | `._._._._` (4 pulses per second)     |
| 4     | Fallback/Error | `----____` (0.5 sec on, 0.5 sec off) |


diagslave -m rtu -b 9600 -p none /dev/ttyUSB0
modpoll -a 1 -0 -r 1000 -t 4 -1 -b 9600 -p none /dev/ttyACM1
