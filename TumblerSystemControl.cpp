#pragma
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "TumblerSystemControl.h"

TumblerSystemControl::TumblerSystemControl(uint8_t Heaterfin, uint8_t Coolerfin)
{   
    /*------DS18B20설정부------*/
    sensors.begin(); // DS18B20 센서 초기화
    sensors.setWaitForConversion(false); // 비동기식으로 온도 측정
    sensors.requestTemperatures(); // 온도 측정 요청
    /*-----출력부 핀 설정-----*/
    pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
    pinMode(Heaterfin, OUTPUT);
    pinMode(Coolerfin, OUTPUT);
    /*------PWM설정부------*/
    pinMode(PWM_PIN, OUTPUT); // PWM 핀 설정
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION); // PWM 설정
    ledcAttachPin(PWM_PIN, PWM_CHANNEL); // PWM 핀과 채널 연결
    ledcWrite(PWM_CHANNEL, 0); // 초기 PWM 값 설정
    TumblerSystemControl::BatteryVoltage = analogRead(0); // 아날로그 핀 0에서 배터리 전압 읽기
    TumblerSystemControl::BatteryVoltagePWM = map(4.2 * TumblerSystemControl::BatteryVoltage / 4095, BATTERY_LOW_VOLTAGE, BATTERY_HIGH_VOLTAGE, 0, 100); // 배터리 전압을 PWM 값으로 변환
    TumblerSystemControl::Coolerfin = Coolerfin;
    TumblerSystemControl::Heaterfin = Heaterfin;
}

TumblerSystemControl::~TumblerSystemControl()
{
}
void TumblerSystemControl::changeControlMode(unsigned char controlMode) //열전소자 제어 함수
{
    switch (controlMode)
    {
    case HEATER_MODE:
        Serial.println("Heater Mode");
        digitalWrite(TumblerSystemControl::Heaterfin, HIGH); // 가열 모드 핀 HIGH
        digitalWrite(TumblerSystemControl::Coolerfin, LOW);  // 냉각 모드 핀 LOW
        break;
    
    case COOLER_MODE:
        Serial.println("Cooler Mode");
        digitalWrite(TumblerSystemControl::Heaterfin, LOW);  // 가열 모드 핀 LOW
        digitalWrite(TumblerSystemControl::Coolerfin, HIGH); // 냉각 모드 핀 HIGH
        break;

    case STANBY_MODE:
        Serial.println("Stanby mode");
        digitalWrite(TumblerSystemControl::Heaterfin, LOW);  // 가열 모드 핀 LOW
        digitalWrite(TumblerSystemControl::Coolerfin, LOW);  // 냉각 모드 핀 LOW
        break;

    case TEMPERATURE_MAINTANENCE_MODE:
        digitalWrite(TumblerSystemControl::Heaterfin, LOW);  // 가열 모드 핀 LOW
        digitalWrite(TumblerSystemControl::Coolerfin, LOW);  // 냉각 모드 핀 LOW
        break;

    case TEMPERATURE_USER_SETTING_MODE:
        digitalWrite(TumblerSystemControl::Heaterfin, LOW);  // 가열 모드 핀 LOW
        digitalWrite(TumblerSystemControl::Coolerfin, LOW);  // 냉각 모드 핀 LOW
        
    default:
        break;
    }
}

void TumblerSystemControl::changeBatteryStatus(bool status) {
    TumblerSystemControl::BatteryStatus = status;
}

unsigned int TumblerSystemControl::BatteryVoltageValue() {
    return int(TumblerSystemControl::BatteryVoltagePWM);
}

int TumblerSystemControl::measureTemperatureRequest() {
    if (sensors.isConversionComplete())
    {
        int value = sensors.getTempCByIndex(0);
        sensors.requestTemperatures();
        return value;
    }
    else
        return;
}

void TumblerSystemControl::systemModeActive(int userSetTemperature) {
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

void TumblerSystemControl::systemModeTempMaint(int userSetTemperature) {
    if (temperatureC < userSetTemperature) // 현재 온도가 설정온도보다 낮을 경우
    {
      changeControlMode(HEATER_MODE); // 가열 모드로 변경
      TumblerSystemControl::pwmValue = map(userSetTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 출력
    }
    else if (TumblerSystemControl::temperatureC > userSetTemperature) // 현재 온도가 설정온도보다 높을 경우
    {
      changeControlMode(COOLER_MODE); // 냉각 모드로 변경
      TumblerSystemControl::pwmValue = map(userSetTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 출력
    }
    else if (TumblerSystemControl::temperatureC == userSetTemperature) // 현재 온도가 설정온도와 같을 경우
    {
      changeControlMode(STANBY_MODE); // 정지 모드로 변경
      ledcWrite(PWM_CHANNEL, 0); // PWM 출력 중지
    }
}

void sysyemModeStandBy() {

}