#pragma once

#ifndef DISPLAYPRINTCONTROL_H
#define DISPLAYPRINTCONTROL_H
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "TumblerSystemControl.h"
 // I2C 핀 설정
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
class DisplayPrintControl : private TumblerSystemControl
{  
private : 
    unsigned int userSetTemperature = 0;
    unsigned int BatteryVoltagePWM = 0;
    int pwmValue = 0;
public :
    /*디스플레이 출력 함수 선언부분*/
    void startingDisplayPrint(); //부팅 Display
    void baseDisplayPrint(); //배경 Display
    void allTumblerDisplayPrint(unsigned int temperatureC); // 일반적인 Display 관리
    void batteryDisplayPrint(); //배터리 관련 Display 관리 함수
    void settingTemperatureDisplayPrint(); //온도 설정 Display 관리 함수
    void endedSettingTemperatureDisplayPrint(); //온도 설정 Display 관리 함수
    void inputUserTemp(float value);
    void inputInfoBattery(unsigned int value);
    void displaySleep(bool status);
    DisplayPrintControl();
    ~DisplayPrintControl() {};
};

#endif