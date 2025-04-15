#pragma once

#ifndef TEMBLERSYSYEMCONTROL_H
#define TEMBLERSYSTEMCONTROL_H
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
/*---시스템 제어 변수---*/
#define STANBY_MODE 0 // 대기 모드
#define ACTIVE_MODE 1 // 활성화 모드
#define TEMPERATURE_MAINTANENCE_MODE 2 // 유지 모드
#define TEMPERATURE_USER_SETTING_MODE 3 // 사용자 온도 변경 모드드
#define BOOTING_MODE 4 // 부팅 모드
/*---전류 방향 제어---*/
#define HEATER_MODE 10 // 가열 모드
#define COOLER_MODE 11 // 냉각 모드

/*-----시스템 한계 온도 설정-----*/
#define SYSTEM_MIN_TEMPERATURE 5 // 시스템 최소 온도 5'C
#define SYSTEM_MAX_TEMPERATURE 80 // 시스템 최대 온도 80'C

/*-----열전소자 전류 제어용 PWM / 출력 PIN 설정부-----*/
#define PWM_FREQ 5000 // PWM 주파수 설정 (5kHz)
#define PWM_RESOLUTION 8 // PWM 해상도 설정 (8비트)
#define PWM_CHANNEL 0 // PWM 채널 설정 (0번 채널 사용)
#define PWM_PIN 1 // PWM 핀 설정 (GPIO 1번 사용)

/*-----배터리 관련 변수 설정-----*/
#define BATTERY_HiGH 0 // 배터리 상태 (상)
#define BATTERY_LOW 1 // 배터리 상태 (하)
#define BATTERY_HIGH_VOLTAGE 4.2 // 배터리 전압 (상)
#define BATTERY_LOW_VOLTAGE 3.0 // 배터리 전압 (하)

/*----------GPIO Define----------*/
#define ONE_WIRE_BUS 4 // DS18B20 센서의 데이터 핀을 GPIO 4번에 연결

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

/*----------시스템 구성성----------*/
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


class TumblerSystemControl
{
protected:  
    unsigned char deviceMode = BOOTING_MODE;
    unsigned char controlMode = BOOTING_MODE; // 초기 모드 설정
    bool BatteryStatus = 0; // 배터리 상태 초기화
    float BatteryVoltage = 0; // 배터리 전압 저장 변수
    float BatteryVoltagePWM = 0; // 배터리 량

private:
    /*---시스템 제어 변수---*/
    unsigned char Heaterfin = 0;
    unsigned char Coolerfin = 0;
    int temperatureC = 0;
    int pwmValue = 0;
    DallasTemperature *sensors;
      

public:
    TumblerSystemControl() {  };
    TumblerSystemControl(uint8_t Heaterfin, uint8_t Coolerfin, DallasTemperature *sensors);
    
    void changeControlMode(unsigned char controlMode);
    void changeBatteryStatus(bool status);
    int measureTemperatureRequest();
    void systemModeActive(int userSetTemperature);
    void systemModeTempMaint(int userSetTemperature);
    void sysyemModeStandBy();
    int TempValue() { return temperatureC; }
    ~TumblerSystemControl();
    unsigned int BatteryVoltageValue();
};

#endif