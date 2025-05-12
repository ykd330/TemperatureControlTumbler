/*-----------include-----------*/
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <FS.h>
#include <LittleFS.h>
//#include "u8g2_font_unifont_t_NanumGothic.h"
//--------------------------------------------------

/*----------전역변수 / 클래스 선언부----------*/
/*-----Display Setting-----*/
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE); // I2C 핀 설정

/*-----Temperature Sensor Setting-----*/
#define ONE_WIRE_BUS 4 // DS18B20 센서의 데이터 핀을 GPIO 4번에 연결
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
int temperatureC = 0; // 현재 온도 저장 변수
int userSetTemperature; // 설정 온도 저장 변수

/*-----시스템 관리 / 제어용-----*/
//GPIO아님
/*---시스템 모드---*/
#define STANBY_MODE 0 // 대기 모드
#define ACTIVE_MODE 1 // 활성화 모드
#define TEMPERATURE_MAINTANENCE_MODE 2 // 유지 모드
#define TEMPERATURE_SETTING_MODE 3
#define DISPLAY_SLEEP 4
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
#define BATTERY_STATUS_FULL 100 // 배터리 완충
#define BATTERY_STATUS_LOW 20 // 배터리 부족
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
volatile bool Trigger = false; // 버튼 트리거 상태 변수
volatile bool Trigger_YN = false; // 버튼 트리거 상태 변수
volatile int TM_count = 0;

/*-----바운싱으로 인한 입력 값 오류 제거용-----*/
volatile unsigned long lastDebounceTime = 0;   // 마지막 디바운스 시간
volatile const unsigned long debounceDelay = 90;          // 디바운싱 지연 시간 (밀리초)

/*-----Display 절전모드 제어용 변수-----*/
float displaySleepTime = 0; // display 절전모드 시간 변수
/*----------전역변수 / 클래스 선언부----------*/



/*----------함수 선언부----------*/
/*------Display / Display Print 제어 함수 설정부------*/
// Display 전반을 모두 수정할 예정 -> 한글 폰트의 크기가 너무 큼 -> 적절한 한글 폰트를 찾지 못함 -> 내용을 압축하여 간결하게 출력해야함
//1. 필요없는 BaseDisplay() 함수 -> batteryDisplay() 함수와 병합
//4. 그림들을 모두 함수로 직접 그리는 방식으로 변경
//5. Battery관련 내용을 출력할 방식을 고안해야함.
/*-----Starting Display Print-----*/
void startingDisplayPrint()
{ 
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("제작 : 5조"))/2, 23, "제작 : 5조"); 
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("Temperature"))/2, 39, "Temperature"); 
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("Control Tumbler"))/2, 55, "Control Tumbler"); 
}
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
/*-----Base DisplayPrint-----*/
void baseDisplayPrint()// 기본 Display 내용 출력 함수
{
  u8g2.drawLine(0, 13, 127, 13); // 가로선 그리기
  if(batteryStatus == BATTERY_STATUS_FULL)
    u8g2.setCursor((u8g2.getDisplayWidth() - u8g2.getUTF8Width("100%")), 12); // 배터리 상태 표시
  else if(batteryStatus == BATTERY_STATUS_LOW) {
    u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getUTF8Width("100%"))) / 2, 12); // 배터리 상태 표시
    u8g2.print("please charge battery"); // 배터리 상태 표시
  }
  else 
    u8g2.setCursor((u8g2.getDisplayWidth() - u8g2.getUTF8Width("99%")), 12); // 배터리 상태 표시
  u8g2.print(BatteryPercentage); // 배터리 상태 표시
  u8g2.print("%"); // 배터리 상태 표시
}

/*-----allTumblerDisplayPrint-----*/
volatile unsigned int AaCo = 0; // 대기 중 카운트 변수
void allTumblerDisplayPrint() //Tumbler Display관리 함수
{
  switch (deviceMode)
  {
    // 대기 모드
  case STANBY_MODE:
    u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("현재 온도")) / 2, 30, "현재 온도"); // 현재 온도 출력
    u8g2.setFont(u8g2_font_unifont_h_symbols); // 폰트 설정
    u8g2.setCursor((u8g2.getDisplayWidth() - u8g2.getUTF8Width("10℃")) / 2, 50); // 현재 온도 출력
    u8g2.print(int(temperatureC)); // 현재 온도 출력
    u8g2.print("℃"); 
    break;
  
    // 온도 조절 모드
  case ACTIVE_MODE:
    u8g2.drawUTF8(0, 30, "온도 조절 중...");
    u8g2.setCursor(2, 47);
    u8g2.print(temperatureC); // 현재 온도 출력
    u8g2.setFont(u8g2_font_unifont_h_symbols); // 폰트 설정
    u8g2.print("℃"); // 현재 온도 출력
    u8g2.setCursor(u8g2.getUTF8Width(" 10℃  ---->  "), 47); // 현재 온도 출력
    u8g2.print(userSetTemperature); // 설정 온도 출력
    u8g2.print("℃"); // 설정 온도 출력
    u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
    //->출력 안됌
    if (AaCo + 1000 <= millis()) {
      u8g2.drawUTF8(u8g2.getUTF8Width("10℃") + 10, 47, "");
    }
    else if (millis() <= AaCo + 1000 and AaCo + 2000 <= millis()) {
      u8g2.drawUTF8(u8g2.getUTF8Width("10℃") + 10, 47, "-");
    }
    else if (millis()< AaCo + 2000 and AaCo + 3000 <= millis()) {
      u8g2.drawUTF8(u8g2.getUTF8Width("10℃") + 10, 47, "--");
    }
    else if (AaCo + 3000 <= millis() and AaCo + 4000 <= millis()) {
      u8g2.drawUTF8(u8g2.getUTF8Width("10℃") + 10, 47, "---");
    }
    else if (AaCo + 4000 <= millis() and AaCo + 5000 <= millis()) {
      u8g2.drawUTF8(u8g2.getUTF8Width("10℃") + 10, 47, "----");
    }
    else if (AaCo + 5000 <= millis() and AaCo + 6000 <= millis()) {
      u8g2.drawUTF8(u8g2.getUTF8Width("10℃") + 10, 47, "---->");
    }
    else 
    {
      AaCo = millis(); // 카운트 초기화
    }
    if (activeMode == HEATER_MODE) 
      u8g2.drawUTF8(0, 63, "가열 중"); // 가열 중 출력
      //2668 if 2615
    
    if (activeMode == COOLER_MODE) 
      u8g2.drawUTF8(0, 63, "냉각 중"); // 냉각 중 출력
      //2744 or 2746
    break;

    // 온도 유지 모드
  case TEMPERATURE_MAINTANENCE_MODE:
    u8g2.drawUTF8(0, 30, "설정온도: ");
    u8g2.setCursor(u8g2.getUTF8Width("설정온도: "), 30); // 설정 온도 출력
    u8g2.print(userSetTemperature); // 설정 온도 출력
    u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도 유지 중"))/2, 45, "온도 유지 중");
    break;
  }
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
}

void settingTemperatureDisplayPrint() //온도 설정 Display 관리 함수
{
  //글자 위치가 이상하게 출력되는 버그 발견 -> 원인 찾을 필요O
  u8g2.setCursor(0,0); // 커서 위치 설정
  u8g2.drawUTF8(0, 16, "설정온도: "); // 설정 온도 출력
  u8g2.setCursor(u8g2.getUTF8Width("설정온도: "), 16);
  u8g2.print(userSetTemperature); // 설정 온도 출력
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("증가:AAA감소:AAA"))/2, 38, "증가:   감소:"); // 온도 설정 : 전원 버튼 출력
  u8g2.drawUTF8(0, 60, "완료: 전원버튼"); // 온도 설정 : 전원 버튼 출력
  u8g2.setFont(u8g2_font_unifont_h_symbols); // 폰트 설정
  u8g2.print("℃"); // 설정 온도 출력
  u8g2.drawUTF8(((u8g2.getDisplayWidth() - u8g2.getUTF8Width("증가:AAA감소:AAA"))/2) + u8g2.getUTF8Width("증가:"), 38, "▲"); // 설정 온도 출력
  u8g2.drawUTF8(((u8g2.getDisplayWidth() - u8g2.getUTF8Width("증가:AAA감소:▼▼▼"))/2) + u8g2.getUTF8Width("증가:▼▼▼") + u8g2.getUTF8Width("감소:") + 30, 38, "▼"); // 설정 온도 출력
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
}

void endedSettingTemperatureDisplayPrint() //온도 설정 Display 관리 함수
{
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도조절을"))/2, 16, "온도조절을"); // 온도 조절을 시작 합니다. 출력
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("시작합니다!"))/2, 32, "시작합니다!"); // 온도 조절을 시작 합니다. 출력
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("화상에"))/2, 48, "화상에"); // 화상에 주의해 주세요! 출력
  u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("주의하세요!"))/2, 64, "주의하세요!"); // 화상에 주의해 주세요! 출력
}
/*-----Main Display Print-----*/


/*------Interrupt 함수 정의 부분------*/
void IRAM_ATTR downButtonF() //Down Button Interrupt Service Routine

{ 
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime > debounceDelay)
  {
    lastDebounceTime = currentTime;
    displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
    downButton = true; // 설정온도 하강 버튼 상태 변수
  }
}
void IRAM_ATTR upButtonF() //Up Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime> debounceDelay)
  {
    lastDebounceTime = currentTime;
    displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
    upButton = true; // 설정온도 상승 버튼 상태 변수
  }
}
void IRAM_ATTR bootButtonF() //Boot Button Interrupt Service Routine
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTime > debounceDelay)
  {
    lastDebounceTime = currentTime;
    displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
    bootButton = true;
  }
}

/*------가열 / 냉각 모드 변경 함수------*/
void changeControlMode(char control_device_mode) //열전소자 제어 함수
{
  if (control_device_mode == HEATER_MODE)
  {
    Serial.println("Heater Mode");
   // ledcWrite(COOLER_CHANNEL, 0); // 초기 PWM 값 설정
    //ledcWrite(HEATER_CHANNEL, pwmValue);
  }
  else if (control_device_mode == COOLER_MODE)
  {
    Serial.println("Cooler Mode");
    //ledcWrite(COOLER_CHANNEL, pwmValue); // 초기 PWM 값 설정
    //ledcWrite(HEATER_CHANNEL, 0);
  }
  else if (control_device_mode == STOP_MODE)
  {
    Serial.println("Stanby mode");
    //ledcWrite(COOLER_CHANNEL, 0); // 초기 PWM 값 설정
    //ledcWrite(HEATER_CHANNEL, 0);
  }
}

void saveUserSetTemperature(int tempToSave) {
  File file = LittleFS.open("/UserTemperature.txt", "w"); // 쓰기 모드("w")로 파일 열기. 파일이 없으면 생성, 있으면 덮어씀.
  if (!file) {
    return;
  }

  file.println(tempToSave); // 파일에 온도 값 쓰기 (println은 줄바꿈 포함)
  file.close();             // 파일 닫기
}

void loadUserSetTemperature() {
  if (LittleFS.exists("/UserTemperature.txt")) { // 파일 존재 여부 확인
    File file = LittleFS.open("/config.txt", "r"); // 읽기 모드("r")로 파일 열기
    if (!file) {
      userSetTemperature = 50; // 기본값 설정
      return;
    }

    if (file.available()) { // 읽을 내용이 있는지 확인
      String tempStr = file.readStringUntil('\n'); // 한 줄 읽기
      userSetTemperature = tempStr.toInt();        // 문자열을 정수로 변환
    } else {
      userSetTemperature = 50; // 기본값 설정
    }
    file.close(); // 파일 닫기
  } 
  else 
  {
    userSetTemperature = 50; // 파일이 없으면 기본값 설정
    saveUserSetTemperature(userSetTemperature); // 기본값으로 파일 새로 생성
  }
}
/*----------함수 선언부----------*/

/*----------시스템----------*/
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
/*----------시스템 구상----------*/


/*----------setup----------*/
void setup()
{
  Wire.begin(); // I2C 초기화
  /*------pinMode INPUT_PULLUP------*/
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLDOWN);
  pinMode(BUTTON_DOWN, INPUT_PULLDOWN);
  pinMode(BUTTON_BOOT, INPUT_PULLDOWN);

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
  u8g2.setPowerSave(0);
  u8g2.setFont(u8g2_font_unifont_t_korean2); // 폰트 설정
  u8g2.setDrawColor(1); // 글자 색상 설정
  u8g2.setFontDirection(0); // 글자 방향 설정
  /*------Interrupt설정부------*/
  attachInterrupt(BUTTON_UP, upButtonF, RISING);
  attachInterrupt(BUTTON_DOWN, downButtonF, RISING);
  attachInterrupt(BUTTON_BOOT, bootButtonF, RISING); //

  /*------FS 설정부------*/
  LittleFS.begin(false); // LittleFS 초기화
  loadUserSetTemperature(); // 설정 온도 불러오기

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
//한글이 포함된 폰트 찾아야 함 -> u8g2_font_unifont_t_korean2로 변경
//motor drive사용 -> 제어 방식 결정 필요

/*----------loop----------*/

void loop()
{ 
  /*----------동작 모드 설정부----------*/
  /*-----loop 지역 변수 선언부-----*/
  static unsigned int saveMode = 0;
  static int AM_count = 0;
  /*Sensors error*/
  if (temperatureC == DEVICE_DISCONNECTED_C) {
    u8g2.clearBuffer();
    u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도 센서 오류"))/2, 30, "온도 센서 오류");
    u8g2.sendBuffer();
    delay(1000);
  }
  
  /*-----온도 측정부-----*/
  if(sensors.isConversionComplete()){
    temperatureC = sensors.getTempCByIndex(0); // 측정온도 저장
    sensors.requestTemperatures(); // 다음 측정을 위해 온도 요청
  }
  
  /*-----Battery 상태 관리 함수-----*/
  //배터리 연결 후 마무리
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

  if(deviceMode == DISPLAY_SLEEP) {
    if (bootButton == true) {
      displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
      bootButton = false;
    }
    if (upButton == true) {
      displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
      upButton = false;
    }
    if (downButton == true) {
      displaySleepTime = millis(); // 버튼이 눌리면 절전모드 해제
      downButton = false;
    }
  }
  
  /*Boot Button*/
  if (bootButton == true && deviceMode != DISPLAY_SLEEP) {
    if ((deviceMode == TEMPERATURE_MAINTANENCE_MODE || deviceMode == TEMPERATURE_SETTING_MODE || deviceMode == ACTIVE_MODE) && Trigger == false) {
      Trigger = true;
      if(deviceMode == ACTIVE_MODE) {
        bootButton = false;
      }
    }
    else if (Trigger == true && Trigger_YN == false) {
      Trigger_YN = true;
    }
    else {
      bootButton = false;
      deviceMode = TEMPERATURE_SETTING_MODE;
    }
  }

  /*Main System control and Display print*/
  //pwmValue = map(userSetTemperature - temperatureC, MIN_TEMPERATURE, MAX_TEMPERATURE, 0, 255);
  //active모드에서 바로 stanby모드로 전환되는 버그 발견 - 수정
  //Active모드에서 종료하기 위한 버튼이 작동X 
  Serial.println(pwmValue);
  u8g2.clearBuffer();
  baseDisplayPrint();
  switch (deviceMode)
  {
  case STANBY_MODE:
    allTumblerDisplayPrint();
    u8g2.sendBuffer();
    break;

  case ACTIVE_MODE:
    allTumblerDisplayPrint();
    if (temperatureC + 2 < userSetTemperature)
      changeControlMode(HEATER_MODE);
    else if (temperatureC - 2 > userSetTemperature)
      changeControlMode(COOLER_MODE);

    if ((temperatureC - userSetTemperature) < 1) {
      AM_count = millis();
      if (AM_count + 5000 < millis()) {
        deviceMode = TEMPERATURE_MAINTANENCE_MODE;
        saveUserSetTemperature(userSetTemperature);
        AM_count = 0;
      }
    }
    //로직 변경 필요 -> active에서 조절 완료 시 온도 조절 모드로 자동으로 변경 / 변경 시점에 설정 온도 저장
    if (upButton == true && Trigger == false) 
    {
      userSetTemperature++;
      upButton = false;
    }
    if (downButton == true && Trigger == false) 
    {
      userSetTemperature--;
      downButton = false;
    }
    if (Trigger == true) 
    { 
      u8g2.clearBuffer();
      baseDisplayPrint();
      u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도 조절을"))/2, 30, "온도 조절을");
      u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("종료하시겠습니까?"))/2, 46, "종료하시겠습니까?");

      if (upButton == true) 
      {
        TM_count++;
        upButton = false;
      }

      if (downButton == true) 
      {
        TM_count--;
        downButton = false;
      }
      if (TM_count < 0) {
        TM_count = 1;
      }
      switch (TM_count % 2)
      {
      case 0:
        u8g2.drawButtonUTF8(40, 63, U8G2_BTN_BW1 | U8G2_BTN_HCENTER, 0, 1, 1, "YES");
        u8g2.drawUTF8(50 + u8g2.getUTF8Width(" "), 63, "/ ");
        u8g2.drawUTF8(50 + u8g2.getUTF8Width(" ") + u8g2.getUTF8Width("/ "), 63, "NO");
        break;
      
      case 1:
        u8g2.drawUTF8(40, 63, "YES");
        u8g2.drawUTF8(50 + u8g2.getUTF8Width(" "), 63, "/ ");
        u8g2.drawButtonUTF8(50 + u8g2.getUTF8Width(" ") + u8g2.getUTF8Width("/ "), 63, U8G2_BTN_BW1 | U8G2_BTN_HCENTER, 0, 1, 1, "NO");
        break;
      }
      
      // 버튼을 눌렀을 때
      // upButton과 downButton이 true일 때 TM_count를 증가 또는 감소시킴

      if (Trigger_YN == true) 
      {
        if (TM_count == 0) 
        {
          u8g2.clearBuffer();
          u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도 조절을"))/2, 30, "온도 조절을");
          u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("종료합니다."))/2, 46, "종료합니다.");
          u8g2.sendBuffer();
          delay(2500);
          deviceMode = STANBY_MODE;
          Trigger = false;
          Trigger_YN = false;
          bootButton = false;
          TM_count = 0;
        }
        else if (TM_count == 1) 
        {
          Trigger = false;
          Trigger_YN = false;
          bootButton = false;
          TM_count = 0;
        }
      }
    }
    u8g2.sendBuffer();
    break;

  case TEMPERATURE_MAINTANENCE_MODE:
    if(Trigger == true) 
    {
      u8g2.clearBuffer();
      baseDisplayPrint();
      u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("온도 유지를"))/2, 30, "온도 유지를");
      u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width("종료하시겠습니까?"))/2, 46, "종료하시겠습니까?");

      switch (TM_count)
      {
      case 0:
        u8g2.drawButtonUTF8(50, 63, U8G2_BTN_BW0 | U8G2_BTN_HCENTER, 0, 1, 1, "YES");
        u8g2.drawUTF8(50 + u8g2.getUTF8Width(" "), 63, "/ ");
        u8g2.drawUTF8(50 + u8g2.getUTF8Width(" ") + u8g2.getUTF8Width("/ "), 63, "NO");
        break;
      
      case 1:
        u8g2.drawUTF8(50, 63, "YES");
        u8g2.drawUTF8(50 + u8g2.getUTF8Width(" "), 63, "/ ");
        u8g2.drawButtonUTF8(50 + u8g2.getUTF8Width(" ") + u8g2.getUTF8Width("/ "), 63, U8G2_BTN_BW0 | U8G2_BTN_HCENTER, 0, 1, 1, "NO");
        break;
      
      case 2:
        TM_count = 0;
      
      case -1:
        TM_count = 1;
      }

      // 버튼을 눌렀을 때
      // upButton과 downButton이 true일 때 TM_count를 증가 또는 감소시킴
      if (upButton == true) 
      {
        TM_count++;
        upButton = false;
      }
      if (downButton == true) 
      {
        TM_count--;
        downButton = false;
      }
      
      if (Trigger_YN == true) {
        if (TM_count == 0) 
        {
          deviceMode = STANBY_MODE;
          Trigger = false;
          Trigger_YN = false;
          bootButton = false;
        }
        else if (TM_count == 1) 
        {
          Trigger = false;
          Trigger_YN = false;
          bootButton = false;
        }
      }
      TM_count = 0;
    }
    else {
      allTumblerDisplayPrint();
      if (temperatureC + 2.5 < userSetTemperature)
        changeControlMode(HEATER_MODE);
      else if (temperatureC - 2.5 > userSetTemperature)
        changeControlMode(COOLER_MODE);
      else
        changeControlMode(STOP_MODE);
    }
    u8g2.sendBuffer();
    break;

  case TEMPERATURE_SETTING_MODE:
    if (Trigger == true) 
    {
      u8g2.clearBuffer();
      endedSettingTemperatureDisplayPrint();
      u8g2.sendBuffer();
      saveUserSetTemperature(userSetTemperature); // 설정 온도 저장
      delay(3000);
      if(userSetTemperature - temperatureC > 0.5) {
        deviceMode = ACTIVE_MODE;
        Trigger = false;
        bootButton = false;
      }
      else if(userSetTemperature - temperatureC <= 0.5) {
        deviceMode = TEMPERATURE_MAINTANENCE_MODE;
        Trigger = false;
        bootButton = false;
      }
      break;
    }
    u8g2.clearBuffer();
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
    u8g2.sendBuffer();
    break;

  
  case DISPLAY_SLEEP:
    if (displaySleepTime + 300000 > millis()) // 버튼이 눌리면 절전모드 해제
    {
      deviceMode = saveMode;
      u8g2.setPowerSave(0); // 절전모드 해제
    }
    break;

  case BOOTING_MODE:
    u8g2.clearBuffer();
    startingDisplayPrint();
    u8g2.sendBuffer();
    delay(3000);
    deviceMode = STANBY_MODE;
    break;
  }


    /*-----Display Low-Energe Mode-----*/
  if (displaySleepTime + 300000 < millis()) // 10초 이상 버튼이 눌리지 않으면 절전모드로 전환
  { 
    saveMode = deviceMode;
    deviceMode = DISPLAY_SLEEP;
    u8g2.setPowerSave(1); // 절전모드 설정
  }
  
  delay(100); // 100ms 대기
}
/*----------loop----------*/
