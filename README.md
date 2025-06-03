한경대학교 2025 졸업작품 PCB
주제 : 온도 조절 텀블러
제작기간 : 2025.03.04 ~ 2025.06.04
제작 팀 : 온도 조절 텀블러 조 (5조) Hardware배치 및 PCB설계 담당 : 유경도 
담당교수 : 김수찬교수님

HardWare spec :
Target-Board : ESP32-C3 SuperMini
Module :
1 x Display : OLED 12864 I2C
1 x Temp Sensors : DS18B20
3 x 3Pin Button (Output / VCC / GND)
1 x Battery Manigement : MAX17043 (1S) (고장으로 사용 불가) 
1 x StepDown Converter : LM2596
1 x Feltier : TEC1-12704
1 x Motor Drive : BTS7960
1 x AC-DC Adaptor (12V 5A)
1 x DC barrel jack (5.5 x 2.1)
1 x Battery : li-po 3000mAh
하드웨어 구성도 : [하드웨어 구성도.pdf](https://github.com/user-attachments/files/20577654/default.pdf)

졸업작품에 사용할 PCB데이터 입니다.
이름 "졸작"이 ESP32-C3 Supermini가 들어갈 메인 PCB입니다. 
각 모듈 (Display, ButtonPCB, H-bridge 드라이버 등)과 연결되는 부분을 패드로 만들거나 헤더 핀 형식으로 만들어 두었습니다.
충전모듈과 배터리, 배터리 센서(fuel guage)는 해당 PCB에 포함될 수 있도록 구성 했습니다.

충전모듈로 5V전압이 걸리면 배터리를 충전하고 동시에 ESP32에 전원을 공급합니다.
H-bridge모듈에 logic level용 5V를 공급하기 위해 패드를 만들어 두었습니다.

점프선만으로 회로를 구성하면 너무 많은 공간과 선이 필요하고 선들이 서로 간섭이 일어나 작동이 원활하게 되지 않는 문제점이 발생하기 때문에 이러한 문제를 제거하고 좁은 텀블러 내부에 회로를 구성하려는 목적으로 설계하였습니다.

이름 "ButtonPCB"는 버튼 모듈이 들어갈 PCB입니다. 
버튼 구조상 버튼 하나당 OUT / VCC / GND에 연결될 3개의 선이 필요한데 총 9개의 선을 배선하기 보단 PCB를 이용해 정리하여 VCC와 GND를 통합하여 선을 줄이는 방식으로 만들고자 하였습니다.

5핀 헤더 형식으로 메인 PCB와 연결되는 구조로 설계 하였으며 배선에 편리함을 가져오기 위한 목적으로 설계하였습니다.
