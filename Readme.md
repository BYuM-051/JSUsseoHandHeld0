Jaesun Project

- HandHeld [BH0, BH1]

BH0 : Makerfabs ESP32 UWB DW3000
- > Connect with BH1 via UART over TTL
- > Connect with BR0 via ESP-NOW over WiFi
- > Connect with BS0 via UWB by DW3000
- > Connect with BR via UWB by DW3000

BH1 : Sunton ESP32-S3 [ESP32-8048S070]
프로젝트 설정 참고 : 
https://www.espboards.dev/esp32/cyd-esp32-8048s070
- > Connect with BH0 via UART over TTL
- > Human UI

BR0 : Makerfabs ESP32 UWB DW3000
- > Connect with BRn via UART over TTL
- > Connect with BH0 via ESP-NOW over WiFi
- > Connect with BH0 via UWB by DW3000
- > Connect with R0 via UART over TTL

R0 Actuating Robotics Module Powered by VEX V5
- > Connect with BR0 via UART over RS485

BRn : Makerfabs ESP32 UWB DW3000
- > Connect with BH0 via UWB by DW3000

BS0 : Makerfabs ESP32 UWB DW3000
- > Connect with BH0 via UWB by DW3000

Address info
ESP32 	Mac Address		UWB ID
BR0 : 		20:43:A8:42:0C:C8		0x100
BR1 : 		C0:5D:89:E9:1C:30		0x101
BR2 : 		20:43:A8:42:0C:CC		0x102
BH0 : 		20:43:A8:42:10:EC		0x200
BH1 :	 	3C:84:27:FC:D8:94		N/A
BS0 : 		C0:5D:89:E9:1C:44		0x300

Ports info
ESP32		RX		TX
BH0		16		17
BH1			

```mermaid
flowchart LR
    %% =========================
    %% AstroShield DFD (Data-Oriented)
    %% =========================

    U[Human User]

    subgraph H[HandHeld]
        BH1[BH1\nESP32-S3\nHuman UI]
        BH0[BH0\nUWB / Comms Controller]
    end

    subgraph R[RoboticsParts]
        R0[R0\nActuating Robotics Module]
        BR0[BR0\nRobot Main Comms Node]
        BR1[BR1\nUWB Anchor Node]
        BR2[BR2\nUWB Anchor Node]
    end

    subgraph S[Shelter]
        BS0[BSn\nShelter UWB Node]
    end

    %% =========================
    %% Human Interaction
    %% =========================
    U -->|mission command / deploy request / confirm input| BH1
    BH1 -->|UI feedback / threat level / system status| U

    %% =========================
    %% Handheld Internal
    %% =========================
    BH1 <-->|UI data / control command / system state| BH0

    %% =========================
    %% ESP-NOW Communication
    %% =========================
    BH0 <-->|mission packet / robot command / sync data| BR0

    %% =========================
    %% UWB Ranging / Positioning
    %% =========================
    BH0 <-->|distance / time-of-flight data| BR0
    BH0 <-->|distance / anchor reference| BR1
    BH0 <-->|distance / anchor reference| BR2
    BH0 <-->|distance / shelter location| BS0

    %% =========================
    %% Robot Control
    %% =========================
    BR0 -->|navigation command / actuator trigger| R0
    R0 -->|execution status / movement state / error| BR0

    %% =========================
    %% Internal Robot Network
    %% =========================
    BR0 -->|anchor sync / localization assist| BR1
    BR0 -->|anchor sync / localization assist| BR2
```