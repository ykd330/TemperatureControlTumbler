/*-----------include-----------*/
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include "U8g2lib.h"
/*-----------include-----------*/
//

/*----------전역변수 / 클래스 선언부----------*/
/*-----Display Setting-----*/
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE); // I2C 핀 설정

/*-----Temperature Sensor Setting-----*/
#define ONE_WIRE_BUS 4 // DS18B20 센서의 데이터 핀을 GPIO 4번에 연결
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float temperatureC = 0; // 현재 온도 저장 변수
float userSetTemperature = 50; // 설정 온도 저장 변수

/*-----시스템 관리 / 제어용-----*/
//GPIO아님
/*---시스템 모드---*/
#define STANBY_MODE 0 // 대기 모드
#define ACTIVE_MODE 1 // 활성화 모드
#define TEMPERATURE_MAINTANENCE_MODE 2 // 유지 모드
#define TEMPERATURE_SETTING_MODE 3
#define SENSOR_ERROR 4
#define DISPLAY_ERROR 5
#define DISPLAY_SLEEP 6
#define BOOTING_MODE 10 // 부팅 모드
unsigned int deviceMode = BOOTING_MODE; // 초기 모드 설정

/*---전류 방향 제어---*/
#define HEATER_MODE 0 // 가열 모드
#define COOLER_MODE 1 // 냉각 모드
#define STOP_MODE 2  // 작동 정지
volatile unsigned char activeMode = false; // 온도 설정 모드 초기화

/*---배터리 관리 설정 부---*/
#define BATTERY_HIGH 0 // 배터리 상태 (상)
#define BATTERY_LOW 1 // 배터리 상태 (하)
#define BATTERY_HIGH_VOLTAGE 4.2 // 배터리 전압 (상)
#define BATTERY_LOW_VOLTAGE 3.0 // 배터리 전압 (하)
#define BATTERY_STATUS_LOW 20
bool batteryStatus = BATTERY_HIGH; // 배터리 상태 초기화
volatile long BatteryVoltage = 0; // 배터리 전압 저장 변수
long BatteryPercentage = 0; // 배터리 량

/*-----GPIO 설정 부-----*/
/*---ESP32-C3 SuperMini GPIO 핀 구성---*/
// GPIO 5 : A5 : MISO /   5V   :    : VCC
// GPIO 6 :    : MOSI /  GND   :    : GND
// GPIO 7 :    : SS   /  3.3V  :    : VCC
// GPIO 8 :    : SDA  / GPIO 4 : A4 : SCK
// GPIO 9 :    : SCL  / GPIO 3 : A3 : 
// GPIO 10:    :      / GPIO 2 : A2 : 
// GPIO 20:    : RX   / GPIO 1 : A1 : 
// GPIO 21:    : TX   / GPIO 0 : A0 : 
//PWM, 통신 관련 핀은 임의로 설정 가능함

/*-----열전소자 전류 제어용 PWM / 출력 PIN 설정부-----*/
#define PWM_FREQ 5000 // PWM 주파수 설정 (5kHz)
#define PWM_RESOLUTION 8 // PWM 해상도 설정 (8비트)
#define COOLER_CHANNEL 0 // PWM 채널 설정 (0번 채널 사용)
#define HEATER_CHANNEL 1  // PWM 채널 설정 (1번 채널 사용)

//#define PWM_PIN 1 // PWM 핀 설정 (GPIO 1번 사용)
#define COOLER_PIN 1 // 냉각 제어
#define HEATER_PIN 2 // 가열 제어

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
volatile bool bootButton = false;
volatile bool upButton = false; // 설정온도 상승 버튼 상태 변수
volatile bool downButton = false; // 설정온도 하강 버튼 상태 변수

/*-----바운싱으로 인한 입력 값 오류 제거용-----*/
volatile unsigned long lastDebounceTime = 0;   // 마지막 디바운스 시간
volatile const unsigned long debounceDelay = 200;          // 디바운싱 지연 시간 (밀리초)

/*-----Display 절전모드 제어용 변수-----*/
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
/* XBM */
// width => 0 ~ 127, 128px, height => 15 ~ 64, 49px
// Height setting : pixel map - px12(0~ ; 12), temp - px 15(15~ ; 30, 45), charging.. etc - px 10 (50~ ; 60) 
// temp width setting : fitst - 19, second - 16, 1px, ":", 1px , temperature number,, Information 
static unsigned char krbits_hyunjae[] = {
  //19 x 12
  0x0e,0x00,0xfd,0x00,0x7d,0xfd,0x3f,0x11,0xfd,0xc0,0x11,0xfd,
  0x0c,0x11,0xfd,0x12,0x11,0xff,0xd2,0x29,0xfd,0x0c,0x29,0xfd,
  0x00,0x29,0xfd,0x02,0x45,0xfd,0x02,0x44,0xfd,0xfc,0x01,0xfd
}; //현재
 

static unsigned char krbits_ondo[] = {
  //16 x 11
  0x00,0x00,0x3c,0x7c,0x42,0x02,0x42,0x02,0x3c,0x02,0x10,0x7c,
  0xff,0x00,0x00,0x10,0x02,0x10,0x7c,0xfe,0x00,0x00
}; //온도
 

 

/*-----Main Display Print-----*/
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
void allTumblerDisplayPrint() //Tumbler Display관리 함수
{
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("현재 온도 :   ℃"))/2, 25, "현재 온도 :   ℃");
  u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getStrWidth("현재 온도 :   ℃"))/2) + u8g2.getStrWidth("현재 온도 : "), 25);
  u8g2.print(temperatureC); // 현재 온도 출력

  switch (deviceMode)
  {
  case STANBY_MODE:
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("설정 온도 :   ℃"))/2, 40, "설정 온도 :   ℃");
    u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getStrWidth("설정 온도 :   ℃"))/2) + u8g2.getStrWidth("설정 온도 : "), 40);
    u8g2.print(userSetTemperature); // 설정 온도 출력
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("대기중..."))/2, 55, "대기중..."); // 대기중... 출력
    break;
  
  case ACTIVE_MODE:
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("목표 온도 :   ℃"))/2, 40, "목표 온도 :   ℃");
    u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getStrWidth("목표 온도 :   ℃"))/2) + u8g2.getStrWidth("목표온도 : "), 40);
    u8g2.print(userSetTemperature); // 설정 온도 출력
    if (activeMode == HEATER_MODE) 
      u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("가열 중"))/2, 55, "가열 중"); // 가열 중 출력
    
    if (activeMode == COOLER_MODE) 
      u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("냉각 중"))/2, 55, "냉각 중"); // 냉각 중 출력
    break;

  case TEMPERATURE_MAINTANENCE_MODE:
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("온도 유지 중"))/2, 55, "온도 유지 중"); // 가열 중 출력
    break;

  default:
    break;
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
  u8g2.setCursor((u8g2.getDisplayWidth() - u8g2.getStrWidth("100%")), 0); // 배터리 상태 표시
  u8g2.print(BatteryPercentage); // 배터리 상태 표시
  u8g2.print("%"); // 배터리 상태 표시
  if(batteryStatus == BATTERY_LOW) // 배터리 상태가 LOW일 경우
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
/*
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
  if (currentTime - lastDebounceTime > debounceDelay)
  {
    lastDebounceTime = currentTime;
    downButton = true; // 설정온도 하강 버튼 상태 변수
  }
}
void IRAM_ATTR upButtonF() //Up Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime> debounceDelay)
  {
    lastDebounceTime = currentTime;
    upButton = true; // 설정온도 상승 버튼 상태 변수
  }
}
void IRAM_ATTR bootButtonF() //Boot Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime > debounceDelay)
  {
    lastDebounceTime = currentTime;
    bootButton != bootButton;
  }
}

/*------가열 / 냉각 모드 변경 함수------*/
void changeControlMode(char control_device_mode) //열전소자 제어 함수
{
  if (control_device_mode == HEATER_MODE)
  {
    Serial.println("Heater Mode");
    ledcWrite(COOLER_CHANNEL, 0); // 초기 PWM 값 설정
    ledcWrite(HEATER_CHANNEL, pwmValue);
  }
  else if (control_device_mode == COOLER_MODE)
  {
    Serial.println("Cooler Mode");
    ledcWrite(COOLER_CHANNEL, pwmValue); // 초기 PWM 값 설정
    ledcWrite(HEATER_CHANNEL, 0);
  }
  else if (control_device_mode == STOP_MODE)
  {
    Serial.println("Stanby mode");
    ledcWrite(COOLER_CHANNEL, 0); // 초기 PWM 값 설정
    ledcWrite(HEATER_CHANNEL, 0);
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
  Wire.begin(); // I2C 초기화
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
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
  u8g2.setFontMode(1); // 폰트 모드 설정
  u8g2.setDrawColor(1); // 글자 색상 설정
  u8g2.setFontDirection(0);

  /*------Interrupt설정부------*/
  attachInterrupt(BUTTON_UP, upButtonF, FALLING);
  attachInterrupt(BUTTON_DOWN, downButtonF, FALLING);
  attachInterrupt(BUTTON_BOOT, bootButtonF, FALLING); //

  /*------PWM설정부------*/
  pinMode(COOLER_PIN, OUTPUT); // PWM 핀 설정
  pinMode(HEATER_PIN, OUTPUT);

  ledcSetup(COOLER_CHANNEL, PWM_FREQ, PWM_RESOLUTION); // PWM 설정
  ledcSetup(HEATER_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(COOLER_PIN, COOLER_CHANNEL); // PWM 핀과 채널 연결
  ledcAttachPin(HEATER_PIN, HEATER_CHANNEL);
  
  ledcWrite(COOLER_CHANNEL, pwmValue); // 초기 PWM 값 설정
  ledcWrite(HEATER_CHANNEL, pwmValue);
}
/*----------setup----------*/

//button이 하드웨어적으로 debouncing이 구현되어 있을 수 있음 -> debouncing 제거 후 test
//한글이 포함된 폰트 찾아야 함
//motor drive사용 x -> mosfet 2개를 사용하는 방식으로 바꿔야함 -> pwm 핀 2개로 제어 -> high / low 상태를 시스템적으로만 정의하고 실제 출력은 pwm만 사용하도록 변경함
//온도 조절을 위해 알맞은 pwm값을 구해야함

/*----------loop----------*/

void loop()
{ 
  /*----------동작 모드 설정부----------*/
  /*-----loop 지역 변수 선언부-----*/
  static unsigned int saveMode = 0;

  /*-----온도 측정부-----*/
  if(sensors.isConversionComplete()){
    temperatureC = sensors.getTempCByIndex(0); // 측정온도 저장
    sensors.requestTemperatures(); // 다음 측정을 위해 온도 요청
  }
  
  /*-----Battery 상태 관리 함수-----*/
  BatteryVoltage = analogRead(0); // 아날로그 핀 0에서 배터리 전압 읽기
  BatteryVoltage = 4.2 * BatteryVoltage / 4095; // 배터리 전압 변환 (0~4.2V)
  BatteryPercentage = map(BatteryVoltage, BATTERY_LOW_VOLTAGE, BATTERY_HIGH_VOLTAGE, 0, 100); // 배터리 전압을 PWM 값으로 변환
  if (BatteryPercentage <= BATTERY_STATUS_LOW)
  {
    batteryStatus = BATTERY_LOW; // 배터리 상태 (하) 설정
  }
  else if (BatteryPercentage > BATTERY_STATUS_LOW)
  {
    batteryStatus = BATTERY_HIGH; // 배터리 상태 (상) 설정
  }

  if (temperatureC == DEVICE_DISCONNECTED_C) {
    deviceMode = SENSOR_ERROR;
  }


  /*Main System control and Display print*/
  pwmValue = map(userSetTemperature - temperatureC, MIN_TEMPERATURE, MAX_TEMPERATURE, 0, 255);
  u8g2.clearBuffer();
  baseDisplayPrint();
  batteryDisplayPrint();
  switch (deviceMode)
  {
  case STANBY_MODE:
  /*Boot Button*/
    if (bootButton == true) {
      deviceMode = TEMPERATURE_SETTING_MODE;
    }
    allTumblerDisplayPrint();
    break;

  case ACTIVE_MODE:
    /*Boot Button*/
    if (bootButton == true) {
      deviceMode = TEMPERATURE_SETTING_MODE;
    }
    allTumblerDisplayPrint();
    if (temperatureC + 1.5 < userSetTemperature)
      changeControlMode(HEATER_MODE);
    else if (temperatureC - 1.5 > userSetTemperature)
      changeControlMode(COOLER_MODE);
    if (upButton == true) 
    {
      userSetTemperature++;
      upButton = false;
    }
    if (downButton == true) 
    {
      userSetTemperature--;
      downButton = false;
    }
    break;

  case TEMPERATURE_MAINTANENCE_MODE:
    /*Boot Button*/
    if (bootButton == true) {
      deviceMode = TEMPERATURE_SETTING_MODE;
    }

    allTumblerDisplayPrint();
    if (temperatureC + 2.5 < userSetTemperature)
      changeControlMode(HEATER_MODE);
    else if (temperatureC - 2.5 > userSetTemperature)
      changeControlMode(COOLER_MODE);
    else
      changeControlMode(STOP_MODE);
    break;

  case TEMPERATURE_SETTING_MODE:
    if (bootButton == false && (userSetTemperature - temperatureC) > 0.5) 
    {
      endedSettingTemperatureDisplayPrint();
      deviceMode = ACTIVE_MODE;
      break;
    }
    else if (bootButton == false && (userSetTemperature - temperatureC) <= 0.5) 
    {
      endedSettingTemperatureDisplayPrint();
      deviceMode = TEMPERATURE_MAINTANENCE_MODE;
      break;
    }

    settingTemperatureDisplayPrint();
    if (upButton == true) 
    {
      userSetTemperature++;
      upButton = false;
    }
    if (downButton == true) 
    {
      userSetTemperature--;
      downButton = false;
    }
    break;

  case SENSOR_ERROR:
    u8g2.clearBuffer();
    u8g2.drawStr(20, 20, "SENSOR ERROR");
    changeControlMode(STOP_MODE);
    if(temperatureC != DEVICE_DISCONNECTED_C);
      deviceMode = STANBY_MODE;
    break;

  case DISPLAY_ERROR:
    Serial.println("Display initialization failed!");
    changeControlMode(STOP_MODE);
    deviceMode = STANBY_MODE;
    delay(1000); // 1초 대기 후 재시도
    break;

  case DISPLAY_SLEEP:
    if (displaySleepTime + 600000 > millis()) // 버튼이 눌리면 절전모드 해제
    {
      deviceMode = saveMode;
      u8g2.setPowerSave(0); // 절전모드 해제
    }
    break;

  case BOOTING_MODE:
    startingDisplayPrint();
    delay(3000);
    deviceMode = STANBY_MODE;
    break;

  default:

    break;
  }


  u8g2.sendBuffer();
    /*-----Display Low-Energe Mode-----*/
  if (displaySleepTime + 600000 < millis()) // 10분분 이상 버튼이 눌리지 않으면 절전모드로 전환
  { 
    saveMode = deviceMode;
    deviceMode = DISPLAY_SLEEP;
    u8g2.setPowerSave(1); // 절전모드 설정
  }




}
/*----------loop----------*/
