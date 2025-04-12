/*-----------include-----------*/
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
/*-----------include-----------*/

/*----------전역변수 / 클래스 선언부----------*/
/*-----Display Setting-----*/
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE); // I2C 핀 설정

/*-----Temperature Sensor Setting-----*/
#define ONE_WIRE_BUS 4 // DS18B20 센서의 데이터 핀을 GPIO 4번에 연결
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
static float temperatureC = 0; // 현재 온도 저장 변수
float userSetTemperature = 50; // 설정 온도 저장 변수

/*-----시스템 관리 / 제어용-----*/
//GPIO아님
#define STANBY_MODE 0 // 대기 모드
#define ACTIVE_MODE 1 // 활성화 모드
#define TEMPERATURE_MAINTANENCE_MODE 2 // 유지 모드기
#define BOOTING_MODE 10 // 부팅 모드
#define HEATER_MODE 3 // 가열 모드
#define COOLER_MODE 4 // 냉각 모드
#define BATTERY_HiGH 5 // 배터리 상태 (상)
#define BATTERY_LOW 6 // 배터리 상태 (하)
char control_mode = BOOTING_MODE; // 초기 모드 설정
bool temperatureSettingMode = false; // 온도 설정 모드 초기화

/*-----GPIO 설정 부-----*/
/*---ESP32-C3 SuperMini GPIO 핀 구성---*/
// GPIO 5 : A5 : MISO /   5V   :    : VCC
// GPIO 6 :    : MOSI /  GND   :    : GND
// GPIO 7 :    : SS   /  3.3V  :    :3.3V
// GPIO 8 :    : SDA  / GPIO 4 : A4 : SCK
// GPIO 9 :    : SCL  / GPIO 3 : A3 : 
// GPIO 10:    :      / GPIO 2 : A2 : 
// GPIO 20:    : RX   / GPIO 1 : A1 : 
// GPIO 21:    : TX   / GPIO 0 : A0 : 
//PWM, 통신 관련 핀은 임의로 설정 가능함

/*-----열전소자 전류 제어용 PWM / 출력 PIN 설정부-----*/
#define PWM_FREQ 5000 // PWM 주파수 설정 (5kHz)
#define PWM_RESOLUTION 8 // PWM 해상도 설정 (8비트)
#define PWM_CHANNEL 0 // PWM 채널 설정 (0번 채널 사용)
#define PWM_PIN 1 // PWM 핀 설정 (GPIO 1번 사용)
#define COOLER_PIN 2 // 냉각 제어
#define HEATER_PIN 3 // 가열 제어

/*-----Push Button 설정부-----*/
#define BUTTON_UP 5   // GPIO 5번에 연결, 설정온도 상승
#define BUTTON_DOWN 6 // GPIO 6번에 연결, 설정온도 하강
#define BUTTON_BOOT 7 // GPIO 7번에 연결, 모드 변경 버튼
float pwmValue = 0; // PWM 출력 값 저장 변수

/*-----시스템 한계 온도 설정-----*/
#define MAX_TEMPERATURE 125 // 최대 온도 125'C
#define MIN_TEMPERATURE -55 // 최소 온도 -55'C
#define SYSTEM_MIN_TEMPERATURE 5 // 시스템 최소 온도 5'C
#define SYSTEM_MAX_TEMPERATURE 80 // 시스템 최대 온도 80'C

/*-----Interrupt 버튼 triger 선언부-----*/
unsigned char bootButton = false;
unsigned char upButton = false; // 설정온도 상승 버튼 상태 변수
unsigned char downButton = false; // 설정온도 하강 버튼 상태 변수

/*-----바운싱으로 인한 입력 값 오류 제거용-----*/
volatile unsigned long lastDebounceTime = 0;   // 마지막 디바운스 시간
const unsigned long debounceDelay = 500;          // 디바운싱 지연 시간 (밀리초)

/*-----Display 절전모드 제어용 변수-----*/
unsigned char displaySleep = false; // display 절전모드 상태 변수
float displaySleepTime = 0; // display 절전모드 시간 변수
/*----------전역변수 / 클래스 선언부----------*/



/*----------함수 선언부----------*/
/*------Display / Display Print 제어 함수 설정부------*/
/*-----Starting Display Print-----*/
void startingDisplayPrint()
{
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("제작 : 5조"))/2, 23, "제작 : 5조"); // Starting... 출력
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("임선진 안대현 유경도"))/2, 38, "임선진 안대현 유경도");
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("Smart Tumbler"))/2, 53, "Smart Tumbler"); // Smart Tumbler 출력
}
/*-----Main Display Print-----*/

void allTumblerDisplayPrint() //Tumbler Display관리 함수
{
  /*
---basic Display---
standby Display :
|Smart Tumbler System|
|--------------------|
| 현재 온도 : XX.X℃  |
| 설정 온도 : XX.X℃  |
|대기중...(..개수 변화)|

active Display :
|Smart Tumbler System|
|--------------------|
|현재 온도 : XX.X℃   |
|목표 온도 : XX.X℃   |
|가열 / 냉각 중       |

temperature maintanence Display :
|Smart Tumbler System|
|--------------------|
|현재 온도 : XX.X℃   |
| 절전 / 가열 / 냉각  |
|온도 유지중...       |
*/
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("현재 온도 :   ℃"))/2, 25, "현재 온도 :   ℃");
  u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getStrWidth("현재 온도 :   ℃"))/2) + u8g2.getStrWidth("현재온도 : "), 25);
  u8g2.print(temperatureC); // 현재 온도 출력

  if (control_mode == STANBY_MODE) {
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("설정 온도 :   ℃"))/2, 40, "설정 온도 :   ℃");
    u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getStrWidth("설정 온도 :   ℃"))/2) + u8g2.getStrWidth("설정온도 : "), 40);
    u8g2.print(userSetTemperature); // 설정 온도 출력
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("대기중..."))/2, 55, "대기중..."); // 대기중... 출력
  }
  else if (control_mode == ACTIVE_MODE) {
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("목표 온도 :   ℃"))/2, 40, "목표 온도 :   ℃");
    u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getStrWidth("목표 온도 :   ℃"))/2) + u8g2.getStrWidth("목표온도 : "), 40);
    u8g2.print(userSetTemperature); // 설정 온도 출력
    if (control_mode == HEATER_MODE) {
      u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("가열 중"))/2, 55, "가열 중"); // 가열 중 출력
    }
    else if (control_mode == COOLER_MODE) {
      u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("냉각 중"))/2, 55, "냉각 중"); // 냉각 중 출력
    }
  }
  else if (control_mode == TEMPERATURE_MAINTANENCE_MODE) {
    if (control_mode == HEATER_MODE) {
      u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("가열 중"))/2, 55, "가열 중"); // 가열 중 출력
    }
    else if (control_mode == COOLER_MODE) {
      u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("냉각 중"))/2, 55, "냉각 중"); // 냉각 중 출력
    }
    else {
      u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("절전 중"))/2, 55, "절전 중"); // 절전 중 출력
    }
  }
}
/*
---Charging / low Battery Display---
Charging Display :
|Smart Tumbler System|
|--------------------|
| 현재 온도 : XX.X℃  |
| 설정 온도 : XX.X℃  |
|    [상태 표시]      |

low-battery Display :
|Smart Tumbler System|
|--------------------|
|  현재 온도 : XX.X℃ |
|  설정 온도 : XX.X℃ |
|  충전이 필요합니다! |
*/
void batteryDisplayPrint() //배터리 관련 Display 관리 함수
{
  
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("충전이 필요합니다!"))/2, 55, "충전이 필요합니다!"); // 충전이 필요합니다! 출력
}

void settingTemperatureDisplayPrint() //온도 설정 Display 관리 함수
{
  /*
---Setting Temperature Display---
|Smart Tumbler System|  
|--------------------|
|  목표 온도 : XX.X℃ |
|온도증가:▲ 온도감소:▼|
|온도 설징 : 전원 버튼|
*/
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("목표 온도 :   ℃"))/2, 25, "목표 온도 :   ℃");
  u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getStrWidth("목표 온도 :   ℃"))/2) + u8g2.getStrWidth("목표온도 : "), 25);
  u8g2.print(userSetTemperature); // 설정 온도 출력
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("온도증가:▲ 온도감소:▼"))/2, 40, "온도증가:▲ 온도감소:▼"); // 온도증가:▲ 온도감소:▼ 출력
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("온도 설징 : 전원 버튼"))/2, 55, "온도 설징 : 전원 버튼"); // 온도 설징 : 전원 버튼 출력
}
void endedSettingTemperatureDisplayPrint() //온도 설정 Display 관리 함수
{
  /*
Ended Setting Display :
|Smart Tumbler System|
|--------------------|
| 목표 온도 : XX.X℃ |
|온도를 조절하는 동안 |
|화상에 주의해 주세요!|
*/
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("목표 온도 :   ℃"))/2, 25, "목표 온도 :   ℃");
  u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getStrWidth("목표 온도 :   ℃"))/2) + u8g2.getStrWidth("목표온도 : "), 25);
  u8g2.print(userSetTemperature); // 설정 온도 출력
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("온도를 조절하는 동안"))/2, 40, "온도를 조절하는 동안"); // 온도를 조절하는 동안 출력
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("화상에 주의해 주세요!"))/2, 55, "화상에 주의해 주세요!"); // 화상에 주의해 주세요! 출력
}
/*-----Smooth Display-----*/
void contrastUpDisplay()// 대비 조정 함수 UP
{
  for (int i = 0; i < 255; i++)
  {
    u8g2.setContrast(i); // 대비 조정
    u8g2.sendBuffer();   // 버퍼 전송
    delay(10);           // 5ms 대기
  }
}
void contrastDownDisplay()// 대비 조정 함수 DOWN
{
  for (int i = 255; i >= 0; i--)
  {
    u8g2.setContrast(i); // 대비 조정
    u8g2.sendBuffer();   // 버퍼 전송
    delay(10);           // 5ms 대기
  }
}

/*-----Base DisplayPrint-----*/
void baseDisplayPrint()// 기본 Display 내용 출력 함수
{
  u8g2.drawStr(0, 8, "Smart Tumbler"); // Smart Tumbler 출력
  u8g2.setFont(u8g2_font_ncenB08_tr); // 폰트 설정
  u8g2.drawLine(0, 13, 127, 13); // 가로선 그리기
}
/*------Interrupt 함수 정의 부분------*/
void IRAM_ATTR downButtonF() //Down Button Interrupt Service Routine
{ 
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime > debounceDelay && temperatureSettingMode == false)
  {
    lastDebounceTime = currentTime;
    downButton = true; // 설정온도 하강 버튼 상태 변수
    displaySleepTime = millis(); // display 절전모드 시간 초기화
  }
  else if (currentTime - lastDebounceTime > debounceDelay && temperatureSettingMode == true )
  {
    lastDebounceTime = currentTime;
    userSetTemperature -= 1; // 설정 온도 하강
    if (userSetTemperature < MIN_TEMPERATURE) // 설정 온도가 최소 온도를 초과할 경우
    {
      userSetTemperature = MIN_TEMPERATURE; // 설정 온도를 최소 온도로 설정
    }
    displaySleepTime = millis(); // display 절전모드 시간 초기화
  }
}
void IRAM_ATTR upButtonF() //Up Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime> debounceDelay && temperatureSettingMode == false)
  {
    lastDebounceTime = currentTime;
    upButton = true; // 설정온도 상승 버튼 상태 변수
    displaySleepTime = millis(); // display 절전모드 시간 초기화
  }
  else if (currentTime - lastDebounceTime > debounceDelay && temperatureSettingMode == true )
  {
    lastDebounceTime = currentTime;
    userSetTemperature += 1; // 설정 온도 상승
    if (userSetTemperature > MAX_TEMPERATURE) // 설정 온도가 최대 온도를 초과할 경우
    {
      userSetTemperature = MAX_TEMPERATURE; // 설정 온도를 최대 온도로 설정
    }
    displaySleepTime = millis(); // display 절전모드 시간 초기화
  }
}
void IRAM_ATTR bootButtonF() //Boot Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime > debounceDelay && control_mode == BOOTING_MODE)
  {
    lastDebounceTime = currentTime;
    bootButton = true;
    displaySleepTime = millis(); // display 절전모드 시간 초기화
  }
  else if (currentTime - lastDebounceTime > debounceDelay && control_mode != BOOTING_MODE)
  {
    lastDebounceTime = currentTime;
    temperatureSettingMode = !temperatureSettingMode; // 온도 설정 모드 상태 변수
    displaySleepTime = millis(); // display 절전모드 시간 초기화
  }
}

/*------가열 / 냉각 모드 변경 함수------*/
void changeControlMode(char control_device_mode) //열전소자 제어 함수
{
  if (control_device_mode == HEATER_MODE)
  {
    Serial.println("Heater Mode");
    digitalWrite(HEATER_PIN, HIGH); // 가열 모드 핀 HIGH
    digitalWrite(COOLER_PIN, LOW);  // 냉각 모드 핀 LOW
  }
  else if (control_device_mode == COOLER_MODE)
  {
    Serial.println("Cooler Mode");
    digitalWrite(HEATER_PIN, LOW);  // 가열 모드 핀 LOW
    digitalWrite(COOLER_PIN, HIGH); // 냉각 모드 핀 HIGH
  }
  else if (control_device_mode == STANBY_MODE)
  {
    Serial.println("Stanby mode");
    digitalWrite(HEATER_PIN, LOW);  // 가열 모드 핀 LOW
    digitalWrite(COOLER_PIN, LOW);  // 냉각 모드 핀 LOW
  }
}
/*----------함수 선언부----------*/

/*----------시스템 구상----------*/
/*---GPIO핀 할당내용---*/
//GPIO 1 : PWM 핀 (OUTPUT) -> 1
//GPIO 2 : 냉각 신호 핀 (OUTPUT) -> 2
//GPIO 3 : 가열 신호 핀 (OUTPUT) -> 3
//GPIO 4 : DS18B20 데이터 핀
//GPIO 5 : 설정온도 상승 버튼 핀
//GPIO 6 : 설정온도 하강 버튼 핀
//GPIO 7 : Booting / Home 버튼 핀
//GPIO 8 : DISPLAY 핀 I2C SCL
//GPIO 9 : DISPLAY 핀 I2C SDA 

/*---Display Setting---*/
// OLED display 설정
//디스플레이 구현 시 고려할 점
  // 부드러운 전환을 위해 0.5초 간격으로 화면을 업데이트
  // 화면 전환 시 이전 화면을 지우고 새로운 화면을 그리는 방식으로 구현
  // 모드당 화면을 나눔 ; 온도 측정 시 화면에 표현
  // 시작시 전용 화면 출력 후
  // 1. 온도 체크 알림 화면
  // 2. 정지 화면 - 메인 화면임 
  // 3. 작동 중 화면 - 현재 온도 / 설정온도 표시 ; 1초 간격으로 업데이트 ; 가열 / 냉각 모드 표시
  // 4. 온도 유지 화면 - 설정 온도를 표시 / "{설정 온도}'C 유지중..." ; 1초 간격으로 업데이트 
  // 배터리 잔량 표시 - 가능시 추가
  // 온도 변환 시 전용 화면으로 전환 ; 전환 트리거 : 전원 버튼, 트리거 발생 시 화면 전환 후 버튼으로 
  // 온도 설정, 전원버튼 한번 더 누를 시 설정 온도에 따라 모드 변환;
//OLED display 함수
/*----------시스템 구상----------*/


/*----------setup----------*/
void setup()
{
  Serial.begin(115200);
  /*------pinMode INPUT_PULLUP------*/
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_BOOT, INPUT_PULLUP);

  /*------pinMode OUTPUT------*/
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);

  /*------DS18B20설정부------*/
  sensors.begin(); // DS18B20 센서 초기화
  sensors.setWaitForConversion(false); // 비동기식으로 온도 측정
  sensors.requestTemperatures(); // 온도 측정 요청
  
  /*------display설정부------*/
  u8g2.begin(); // display 초기화
  u8g2.enableUTF8Print(); // UTF-8 문자 인코딩 사용
  if (!u8g2.begin())
  {
    Serial.println("Display initialization failed!");
    for (;;); // 초기화 실패 시 무한 루프
  }
  
  u8g2.setFont(u8g2_font_ncenB08_tr); // 폰트 설정
  u8g2.setFontMode(1); // 폰트 모드 설정
  u8g2.setDrawColor(1); // 글자 색상 설정

  /*------Interrupt설정부------*/
  attachInterrupt(BUTTON_UP, upButtonF, FALLING);
  attachInterrupt(BUTTON_DOWN, downButtonF, FALLING);
  attachInterrupt(BUTTON_BOOT, bootButtonF, FALLING); //

  /*------PWM설정부------*/
  pinMode(PWM_PIN, OUTPUT); // PWM 핀 설정
  //ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION); // PWM 설정
  //ledcAttachPin(PWM_PIN, PWM_CHANNEL); // PWM 핀과 채널 연결
  
  ledcWrite(PWM_CHANNEL, pwmValue); // 초기 PWM 값 설정
}
/*----------setup----------*/

/*----------loop----------*/
void loop()
{ 
  u8g2.clearBuffer();
  /*------Starting DisplayPrint------*/
  if(millis() < 3000) {
    baseDisplayPrint();
    startingDisplayPrint();
    u8g2.sendBuffer();
  }
  /*----------동작 모드 설정부----------*/
  /*-----loop 지역 변수 선언부-----*/
  static float lastTemperature = 0; // 마지막 온도 저장 변수
  temperatureC = 50;
  volatile static float setTemperature = userSetTemperature;
  volatile static char lastmode = STANBY_MODE; // 마지막 모드 저장 변수

  /*-----온도 측정부-----*/
  if(sensors.isConversionComplete()){
    temperatureC = sensors.getTempCByIndex(0); // 측정온도 저장
    sensors.requestTemperatures(); // 다음 측정을 위해 온도 요청
  }
  /*-----온도 센서 오류 발생 시 오류 메세지 출력-----*/
  if(temperatureC == DEVICE_DISCONNECTED_C)
  {
    u8g2.println("센서에 문제가 있어요");
    Serial.println("센서에 문제가 있어요");
    delay(3);
    while(1) {
    u8g2.clearBuffer();
    if(temperatureC != DEVICE_DISCONNECTED_C)
      break; // 센서가 정상적으로 연결되면 루프 종료
    u8g2.setCursor(0, 30);
    u8g2.print("센서 감지중");
    u8g2.print("센서 감지중.");
    u8g2.print("센서 감지중..");
    u8g2.print("센서 감지중...");
    u8g2.sendBuffer();
    delay(500); // 0.5초 대기 후 다시 시도
    }
  }


  /*------Main System Setting------*/
  /*-----Main Display for User-----*/
  if (temperatureSettingMode == true) {
    static bool temperatureSettingModeFlag = temperatureSettingMode; // 온도 설정 모드 상태 변수
    if (temperatureSettingModeFlag != temperatureSettingMode) {

    }
    contrastDownDisplay(); // 대비 조정
    u8g2.clearBuffer(); // display 버퍼 초기화
    baseDisplayPrint(); // display 기본 출력
    settingTemperatureDisplayPrint(); // 온도 설정 화면 출력
    u8g2.sendBuffer(); // display 버퍼 전송
    contrastUpDisplay(); // 대비 조정
  }
  else if (temperatureSettingMode == false) {
    contrastDownDisplay();
    u8g2.clearBuffer();
    baseDisplayPrint(); // display 기본 출력
    batteryDisplayPrint(); // display 기본 출력
    u8g2.sendBuffer(); // display 버퍼 전송
    contrastUpDisplay(); // 대비 조정
  }
  
  
  /*-----PWM 설정부 / 동작-----*/


  /*-----ACTIVE_MODE 동작-----*/
  if (control_mode == ACTIVE_MODE) // 활성화 모드일 경우                                                                                                                                                                                                                                    ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                
  {
    if (temperatureC < userSetTemperature) // 현재 온도가 설정온도보다 낮을 경우
    {
      changeControlMode(HEATER_MODE); // 가열 모드로 변경
      pwmValue = map(userSetTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 출력
    }
    else if (temperatureC > userSetTemperature) // 현재 온도가 설정온도보다 높을 경우
    {
      changeControlMode(COOLER_MODE); // 냉각 모드로 변경
      pwmValue = map(userSetTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 출력
    }
    else if (temperatureC == userSetTemperature) // 현재 온도가 설정온도와 같을 경우
    {
      changeControlMode(STANBY_MODE); // 정지 모드로 변경
      ledcWrite(PWM_CHANNEL, 0); // PWM 출력 중지
    }
  }

  /*-----KEEP_TEMPERATURE_MODE 동작-----*/
  if(control_mode == TEMPERATURE_MAINTANENCE_MODE) // 유지 모드일 경우
  {
    if (temperatureC < userSetTemperature) // 현재 온도가 설정온도보다 낮을 경우
    {
      changeControlMode(HEATER_MODE); // 가열 모드로 변경
      pwmValue = map(userSetTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 출력
    }
    else if (temperatureC > userSetTemperature) // 현재 온도가 설정온도보다 높을 경우
    {
      changeControlMode(COOLER_MODE); // 냉각 모드로 변경
      pwmValue = map(userSetTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 출력
    }
    else if (temperatureC == userSetTemperature) // 현재 온도가 설정온도와 같을 경우
    {
      changeControlMode(STANBY_MODE); // 정지 모드로 변경
      ledcWrite(PWM_CHANNEL, 0); // PWM 출력 중지
    }
  }

  /*-----Display Low-Energe Mode-----*/
  if (displaySleepTime + 10000 < millis()) // 10초 이상 버튼이 눌리지 않으면 절전모드로 전환
  {
    displaySleep = true; // 절전모드 상태 변수 설정
    contrastDownDisplay(); // 대비 조정
    u8g2.setPowerSave(1); // 절전모드 설정
    contrastUpDisplay(); // 대비 조정
  }
  else if (displaySleepTime + 10000 > millis()) // 버튼이 눌리면 절전모드 해제
  {
    displaySleep = false; // 절전모드 해제
    contrastDownDisplay(); // 대비 조정
    u8g2.setPowerSave(0); // 절전모드 해제
    contrastUpDisplay(); // 대비 조정
  }
}
/*----------loop----------*/
