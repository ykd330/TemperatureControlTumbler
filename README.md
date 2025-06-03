Code Editor 형식으로 변환해서 보시는 걸 추천합니다.

한경대학교 2025 졸업작품 SoftWare
주제 : 온도 조절 텀블러
제작기간 : 2025.03.04 ~ 2025.05.28
제작 팀 : 온도 조절 텀블러 조 (5조)
SoftWare설계 및 제작 담당 : 유경도
담당교수 : 김수찬교수님

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

GPIO_PIN
COOLER_PWM_PIN = 1, // PWM
HEATER_PWM_PIN = 2, // PWM
ONE_WIRE_BUS = 3,   // DS18B20 센서 핀
BUTTON_BOOT = 5,    // 모드 변경 버튼
BUTTON_UP = 6,      // 설정온도 상승 버튼
BUTTON_DOWN = 7,    // 설정온도 하강 버튼
SDA_I2C = 8,        // Hardware에서 설정된 I2C핀
SCL_I2C = 9,        // Hardware에서 설정된 I2C핀
COOLING_PAN = 10,
COOLER_PIN = 20,    // 냉각 제어 핀
HEATER_PIN = 21     // 가열 제어 핀

구현 기능 : 
코드 흐름도 : [코드 흐름도.pdf](https://github.com/user-attachments/files/20577649/default.pdf)

Device Mode :
0. Boot Mode :
최초 전력 공급 시 자동으로 진입되는 모드
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

프로젝트 진행 결과 및 개인적 고찰 :
2025.06.04 :
결과 : 냉각기능 구현 실패 / 가열기능 약 62℃까지 가능 (단열 시 증가 여지 있음)
고찰 :
1. 펠티어소자의 발열량이 생각보다 매우 크므로 이를 제어할 수 있는 냉각 솔루션을 구현해야한다.
2. 펠티어소자의 발열량과는 별개로 텀블러에서 받아들이는 열량이 생각보다 적다. 이를 해결하지 못하면 빠르게 물의 온도를 올리는 것은 불가능할 것으로 보인다.
-> 텀블러의 소재 변경이나 switching 방식으로 여러 펠티어소자를 부착하여 시간마다 발열점을 바꾸어 주는 방식, 고 열전도 물질을 사용하여 텀블러 외부를 감싸고 내부에 써멀 패드나 써멀 그리스를 사용하여 열전도율을 최대한 끌어내는 방식 고려중
3. 텀블러 내부에 사용할 수 있는 공간이 매우 적다. 최대한 효율적인 배치로 쿨링 팬의 공기의 흐름을 방해하지 않으면서 배선하기 쉽지 않으므로 외부 설계 시 이러한 점을 최대한 고려해야 함을 알았다.

