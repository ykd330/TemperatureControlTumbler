한경대학교 2025 졸업작품 SoftWare
주제 : 온도 조절 텀블러
제작기간 : 2025.03.04 ~ 2025.05.28
제작 팀 : 온도 조절 텀블러 조 (5조)
SoftWare설계 및 제작 담당 : 유경도
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

Development Environment :
Dev tool : VSCode (Platform IO) + Arduino IDE(Test 및 Serial통신) <- VSCode에서 CDC boot 옵션 설정이 제공되지 않아 임의의 Serial통신이 불가능
Simulation : Wokwi Simulator(ESP32-C3)

used Library : 
1. arduino.h 
2. u8g2.h (Display)
3. Wire.h (I2C)
4. OneWire (Sensors Transmission)
5. DallasTemperature (Sensors Control)
6. FS.h / LittleFS.h (File Save )
7. SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h (FuelGauge)

used GPIO Pin :
ULT_PIN = 1,            //오류 핀
EEP_PIN = 2,            //전류 제어 핀
ONE_WIRE_BUS = 3,       // DS18B20 센서 핀
BUTTON_BOOT = 5,        // 모드 변경 버튼
BUTTON_UP = 6,          // 설정온도 상승 버튼
BUTTON_DOWN = 7,        // 설정온도 하강 버튼
SDA_I2C = 8,            // Hardware에서 설정된 I2C핀
SCL_I2C = 9,            // Hardware에서 설정된 I2C핀
COOLER_PIN = 20,        // 냉각 제어 핀
HEATER_PIN = 21         // 가열 제어 핀

구현 기능 : 
코드 흐름도 : [코드 흐름도.pdf](https://github.com/user-attachments/files/20577649/default.pdf)

Device Mode :
0. Boot Mode :
MPU에 최초 전력 공급 시 자동으로 진입되는 모드
약 3초 후 자동으로 Stanby Mode로 진입

1. Stanby Mode :
Feltier : 온도 제어 X
Display : 현재 온도, 배터리 상태만 출력
*Boot 버튼을 통해 Temperature

2. Active Mode : 
Feltier : 현재온도와 설정온도를 비교하여 가열 / 냉각 수행
Display : 현재온도, 목표온도, 배터리 잔량(%), 온도변화 애니메이션, 가열 / 냉각 출력
*온도 조절이 종료되면 자동으로 T.M Mode로 진입
*Up / Down 버튼을 통해 작동 중에도 설정온도 변경이 가능
*Boot 버튼을 통해 종료 가능

3. Temperature Maintanence Mode :
Feltier : 현재온도와 설정온도의 차이가 심해지면 작동하여 온도 유지
Display : 현재온도, 배터리 상태 출력

4. Temperature Setting Mode :
Display : 목표 온도 표기 / 전용 Display 출력 화면
Button : Up / Down 버튼으로 설정온도를 조절, Boot Button으로 온도를 확정
*함수 종료시 설정된 온도를 기준으로 Active 모드와 T.M 모드를 자동으로 선택하여 작동
*Boot 버튼을 통해 종료 가능

5. Sleep Display :
약 5분동안 버튼의 입력이 없으면 자동으로 Display는 절전모드로 진입
버튼 입력 시 절전모드 해제 / 절전모드 상태에서 버튼 입력은 절전모드 해제에만 사용됨

6. reboot :
boot버튼을 5초 이상 누르면 강제로 재부팅함

H-Bridge Control :
1. Logic 핀 2개와 PWM핀 2개로 제어
2. 드라이버 특성상 두개의 logic 핀은 작동시 high로 설정
3. PWM핀을 통해 전류의 흐름 및 전류량을 제어함
4. 단 두 PWM핀에 동시에 PWM이 인가될 경우 드라이버 과열로 손상 가능성 있으므로 소프트웨어적으로 delay를 추가함

