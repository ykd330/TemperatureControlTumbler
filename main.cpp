/*-----------include-----------*/
#include <Arduino.h>
#include "DisplayPrintControl.h"
#include "TumblerSystemControl.h"
#include <OneWire.h>
#include <DallasTemperature.h>
/*-----------include-----------*/

/*----------GPIO Define----------*/
#define ONE_WIRE_BUS 4 // DS18B20 센서의 데이터 핀을 GPIO 4번에 연결
#define BUTTON_UP 5   // GPIO 5번에 연결, 설정온도 상승
#define BUTTON_DOWN 6 // GPIO 6번에 연결, 설정온도 하강
#define BUTTON_BOOT 7 // GPIO 7번에 연결, 모드 변경 버튼

#define HEAT_PIN 2
#define COOL_PIN 3
/*----------GPIO Define----------*/

/*----------전역변수 / 클래스 선언부----------*/
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
TumblerSystemControl sysControl(HEAT_PIN, COOL_PIN, &sensors);
DisplayPrintControl Display;
int userSetTemperature = 0; // 설정 온도 저장 변수

/*-----시스템 관리 / 제어용-----*/
#define STANBY_MODE 0 // 대기 모드
#define ACTIVE_MODE 1 // 활성화 모드
#define TEMPERATURE_MAINTANENCE_MODE 2 // 유지 모드
#define TEMPERATURE_USER_SETTING_MODE 3 // 사용자 온도 변경 모드

#define BOOTING_MODE 4 // 부팅 모드
volatile unsigned char Mode = BOOTING_MODE;

/*---시스템 모드---*/


/*-----Push Button 설정부-----*/
/*-----Interrupt 버튼 triger 선언부-----*/
volatile bool bootButton = false;
volatile bool upButton = false; // 설정온도 상승 버튼 상태 변수
volatile bool downButton = false; // 설정온도 하강 버튼 상태 변수
volatile bool TemperatureSettingMode = false; // 온도 설정 모드 초기화

/*-----바운싱으로 인한 입력 값 오류 제거용-----*/
volatile unsigned long lastDebounceTime = 0;   // 마지막 디바운스 시간
const unsigned long debounceDelay = 200;          // 디바운싱 지연 시간 (밀리초)

/*-----Display 절전모드 제어용 변수-----*/
float displaySleepTime = 0; // display 절전모드 시간 변수
/*----------전역변수 / 클래스 선언부----------*/

/*----------함수 선언부----------*/
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
    bootButton = true;
  }
}

/*----------setup----------*/
void setup()
{
  Serial.begin(115200);
  sensors.begin();
  /*------pinMode INPUT_PULLUP------*/
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_BOOT, INPUT_PULLUP);
  /*------display설정부------*/

  /*------Interrupt설정부------*/
  attachInterrupt(BUTTON_UP, upButtonF, FALLING);
  attachInterrupt(BUTTON_DOWN, downButtonF, FALLING);
  attachInterrupt(BUTTON_BOOT, bootButtonF, FALLING); //

}
/*----------setup----------*/

/*----------loop----------*/

void loop()
{ 
  u8g2.clearBuffer();
  /*------Starting DisplayPrint------*/
  if(Mode == BOOTING_MODE) {
    Display.baseDisplayPrint();
    Display.startingDisplayPrint();
    u8g2.sendBuffer();
  }
  /*----------동작 모드 설정부----------*/
  /*-----loop 지역 변수 선언부-----*/
  static float lastTemperature = 0; // 마지막 온도 저장 변수
  volatile static float setTemperature = userSetTemperature;

  /*-----Battery 상태 관리 함수-----*/
  if (sysControl.BatteryVoltageValue() <= 20)
  {
    sysControl.changeBatteryStatus(LOW); // 배터리 상태 (하) 설정
  }
  else if (sysControl.BatteryVoltageValue() > 20)
  {
    sysControl.changeBatteryStatus(HIGH); // 배터리 상태 (상) 설정
  }

  static bool endedChange = false;
  /*------Main System Setting------*/
  /*-----Main Display for User-----*/
  u8g2.clearBuffer(); // display 버퍼 초기화
  Display.baseDisplayPrint(); // display 기본 출력
  Display.batteryDisplayPrint(); // 배터리 상태 출력
  if (Mode == TemperatureSettingMode) {
    sysControl.changeControlMode(Mode);
    Display.settingTemperatureDisplayPrint();
    if (endedChange)
    {
      u8g2.clearBuffer(); // display 버퍼 초기화
      Display.baseDisplayPrint(); // display 기본 출력
      Display.batteryDisplayPrint(); // 배터리 상태 출력
      Display.endedSettingTemperatureDisplayPrint();
      u8g2.sendBuffer();
      if (userSetTemperature - 2 <= sysControl.measureTemperatureRequest() || userSetTemperature + 2 >= sysControl.measureTemperatureRequest())
      { 
        Mode = ACTIVE_MODE;
        endedChange = false;
      }
    }
    u8g2.sendBuffer();
  }
  
    /*-----Button Interrupt Reset-----*/
    if (bootButton == true) // 부팅 버튼이 눌리면
    { 
      Serial.println("bootButton"); // 디버깅용 출력
      if(Mode = TemperatureSettingMode && bootButton == true) {
        endedChange = true;
      }
      else
        Mode = TemperatureSettingMode;
      displaySleepTime = millis(); // display 절전모드 시간 초기화
      bootButton = false; // 부팅 버튼 상태 변수 초기화
    }

    if (upButton == true) // 설정온도 상승 버튼이 눌리면
    {
      upButton = false; // 설정온도 상승 버튼 상태 변수 초기화
      Serial.println("upButton"); // 디버깅용 출력
      displaySleepTime = millis(); // display 절전모드 시간 초기화
    }

    if (downButton == true) // 설정온도 하강 버튼이 눌리면
    {
      downButton = false; // 설정온도 하강 버튼 상태 변수 초기화
      Serial.println("downButton"); // 디버깅용 출력
      displaySleepTime = millis(); // display 절전모드 시간 초기화
    }
  /*-----PWM 설정부 / 동작-----*/


  /*-----ACTIVE_MODE 동작-----*/ //switch문으로 변경 필요
  /*-----KEEP_TEMPERATURE_MODE 동작-----*/

    /*-----Display Low-Energe Mode-----*/
  if (displaySleepTime + 10000 < millis()) // 10초 이상 버튼이 눌리지 않으면 절전모드로 전환
  {
    Display.displaySleep(true);
  }
  else if (displaySleepTime + 10000 > millis()) // 버튼이 눌리면 절전모드 해제
  {
    Display.displaySleep(false);
  }

}
/*----------loop----------*/
