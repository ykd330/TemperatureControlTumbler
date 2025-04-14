#pragma 
#include "DisplayPrintControl.h"
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

void DisplayPrintControl::startingDisplayPrint() {
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("제작 : 5조"))/2, 23, "제작 : 5조"); // Starting... 출력
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("임선진 안대현 유경도"))/2, 38, "임선진 안대현 유경도");
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("Smart Tumbler"))/2, 53, "Smart Tumbler"); // Smart Tumbler 출력
}

void DisplayPrintControl::allTumblerDisplayPrint(unsigned int temperatureC) //Tumbler Display관리 함수
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
  u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getStrWidth("현재 온도 :   ℃"))/2) + u8g2.getStrWidth("현재 온도 : "), 25);
  u8g2.print(temperatureC); // 현재 온도 출력

  if (TumblerSystemControl::controlMode == STANBY_MODE) {
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("설정 온도 :   ℃"))/2, 40, "설정 온도 :   ℃");
    u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getStrWidth("설정 온도 :   ℃"))/2) + u8g2.getStrWidth("설정 온도 : "), 40);
    u8g2.print(userSetTemperature); // 설정 온도 출력
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("대기중..."))/2, 55, "대기중..."); // 대기중... 출력
  }
  else if (TumblerSystemControl::controlMode == ACTIVE_MODE) {
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("목표 온도 :   ℃"))/2, 40, "목표 온도 :   ℃");
    u8g2.setCursor(((u8g2.getDisplayWidth() - u8g2.getStrWidth("목표 온도 :   ℃"))/2) + u8g2.getStrWidth("목표온도 : "), 40);
    u8g2.print(userSetTemperature); // 설정 온도 출력
    if (TumblerSystemControl::controlMode == HEATER_MODE) {
      u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("가열 중"))/2, 55, "가열 중"); // 가열 중 출력
    }
    else if (TumblerSystemControl::controlMode == COOLER_MODE) {
      u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("냉각 중"))/2, 55, "냉각 중"); // 냉각 중 출력
    }
  }
  else if (TumblerSystemControl::controlMode == TEMPERATURE_MAINTANENCE_MODE) {
    if (TumblerSystemControl::controlMode == HEATER_MODE) {
      u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("가열 중"))/2, 55, "가열 중"); // 가열 중 출력
    }
    else if (TumblerSystemControl::controlMode == COOLER_MODE) {
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
void DisplayPrintControl::batteryDisplayPrint() //배터리 관련 Display 관리 함수
{
  u8g2.setCursor((u8g2.getDisplayWidth() - u8g2.getStrWidth("100%")), 0); // 배터리 상태 표시
  u8g2.print(TumblerSystemControl::BatteryVoltagePWM); // 배터리 상태 표시
  u8g2.print("%"); // 배터리 상태 표시
  if(TumblerSystemControl::BatteryStatus == BATTERY_LOW) // 배터리 상태가 LOW일 경우
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("충전이 필요합니다!"))/2, 55, "충전이 필요합니다!"); // 충전이 필요합니다! 출력
}

void DisplayPrintControl::settingTemperatureDisplayPrint() //온도 설정 Display 관리 함수
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
  u8g2.print(DisplayPrintControl::userSetTemperature); // 설정 온도 출력
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("온도증가:▲ 온도감소:▼"))/2, 40, "온도증가:▲ 온도감소:▼"); // 온도증가:▲ 온도감소:▼ 출력
  u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getStrWidth("온도 설징 : 전원 버튼"))/2, 55, "온도 설징 : 전원 버튼"); // 온도 설징 : 전원 버튼 출력
}
void DisplayPrintControl::endedSettingTemperatureDisplayPrint() //온도 설정 Display 관리 함수
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

/*-----Base DisplayPrint-----*/
void DisplayPrintControl::baseDisplayPrint()// 기본 Display 내용 출력 함수
{
  u8g2.drawStr(0, 8, "Smart Tumbler"); // Smart Tumbler 출력
  u8g2.setFont(u8g2_font_ncenB08_tr); // 폰트 설정
  u8g2.drawLine(0, 13, 127, 13); // 가로선 그리기
}

void DisplayPrintControl::inputUserTemp(float value){
    userSetTemperature = value;
}

void DisplayPrintControl::inputInfoBattery(unsigned int value) {
    BatteryVoltagePWM = value;
}

void DisplayPrintControl::displaySleep(bool status) {
    u8g2.setPowerSave(status);
}

DisplayPrintControl::DisplayPrintControl() {
    Wire.begin(); // I2C 초기화
    u8g2.begin(); // display 초기화
    u8g2.enableUTF8Print(); // UTF-8 문자 인코딩 사용
    u8g2.setFont(u8g2_font_ncenB08_tr); // 폰트 설정
    u8g2.setFontMode(1); // 폰트 모드 설정
    u8g2.setDrawColor(1); // 글자 색상 설정
}