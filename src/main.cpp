/*-------include-------*/
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/*------소자 설정------*/

/*-----Display Setting-----*/
// OLED display 설정
#define SCREEN_WIDTH 128         // display 가로
#define SCREEN_HEIGHT 64         // display 세로
#define OLED_RESET -1            // reset pin
#define SSD1306_I2C_ADDRESS 0x3C // I2C 주소
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//OLED display 함수
void mainDisplayPrint(); // display 함수 선언부
void DisplayPrintNStop();
void DisplayPrintNActive(); 
void DisplayPrintNKeep(); 
void displayPrint(const char *text);

/*-----Temperature Sensor Setting-----*/
// DS18B20 온도 감지 센서 설정
#define ONE_WIRE_BUS 4 // DS18B20 센서의 데이터 핀을 GPIO 4번에 연결
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float temperatureC = 0; // 현재 온도 저장 변수
// SYSTEM 기본 온도 설정
float setTemperature = 30;

/*-----작동 모드-----*/
//시스템 작동 모드 설정
#define STOP_MODE 0 // 정지 모드
#define ACTIVE_MODE 1 // 활성화 모드
#define KEEP_TEMPERATURE_MODE 2 // 유지 모드
volatile static char user_control_mode = STOP_MODE; // 사용자 선택 모드 저장 변수 / 기본 : 정지 모드
//열전소자 작동 모드 설정 
#define HEATER_MODE 3 // 가열 모드
#define COOLER_MODE 4 // 냉각 모드
char control_mode = STOP_MODE; // 초기 모드 설정

/*-----GPIO Pin 설정부-----*/
// Interrupt 핀 설정
#define BUTTON_UP 5   // GPIO 5번에 연결, 설정온도 상승
#define BUTTON_DOWN 6 // GPIO 6번에 연결, 설정온도 하강

// 모드 변경 버튼 설정
#define BUTTON_ENTER 7 // GPIO 7번에 연결, 모드 변경 버튼
volatile static char lastControlMode = STOP_MODE; // 마지막 모드 저장 변수

// 열전소자 모드 제어
#define HEATER_PIN 10 // 가열 모드
#define COOLER_PIN 11 // 냉각 모드
#define ACTIVE_PIN 12 // 활성화 핀 (가열/냉각 모드에 따라 설정)



/*-----시스템 한계 온도 설정-----*/
#define MAX_TEMPERATURE 125 // 최대 온도 125'C
#define MIN_TEMPERATURE -55 // 최소 온도 -55'C
#define SYSTEM_MIN_TEMPERATURE 15 // 시스템 최소 온도 15'C
#define SYSTEM_MAX_TEMPERATURE 85 // 시스템 최대 온도 85'C

//PWM 설정
#define PWM_FREQ 5000 // PWM 주파수 설정 (5kHz)
#define PWM_RESOLUTION 8 // PWM 해상도 설정 (8비트)
#define PWM_CHANNEL 0 // PWM 채널 설정 (0번 채널 사용)
#define PWM_PIN 2 // PWM 핀 설정 (GPIO 2번 사용)
float pwmValue = 0;

// Interrupt 버튼 함수 선언부
void IRAM_ATTR Button_up();
void IRAM_ATTR Button_down();
void IRAM_ATTR Enter_func();
void IRAM_ATTR Active(); // 활성화 핀에 대한 인터럽트 함수
void changeControlMode(char control_mode); // 모드 변경 함수

/*-----바운싱으로 인한 입력 값 오류 제거용-----*/
volatile unsigned long lastDebounceTimeUp = 0;   // BUTTON_UP 디바운싱 시간
volatile unsigned long lastDebounceTimeDown = 0; // BUTTON_DOWN 디바운싱 시간
volatile unsigned long lastDebounceTimeMode = 0; // BUTTON_MODE 디바운싱 시간
volatile unsigned long lastActiveTime = 0;       // ACTIVE_PIN 디바운싱 시간
const unsigned long debounceDelay = 50;          // 디바운싱 지연 시간 (밀리초)

// active pin 설정
char activecontrol = 0; // 활성화 핀 상태 변수

/*
---GPIO핀 사용 현황---
GPIO 3 : PWM 핀
GPIO 4 : DS18B20 데이터 핀
GPIO 5 : 설정온도 상승 버튼 핀
GPIO 6 : 설정온도 하강 버튼 핀
GPIO 7 : 모드 변경 버튼 핀
GPIO 8 : DISPLAY 핀 I2C SCL
GPIO 9 : DISPLAY 핀 I2C SDA 
GPIO 10 : 가열 모드 핀 (OUTPUT)
GPIO 11 : 냉각 모드 핀 (OUTPUT)
GPIO 12 : 시스템 설정 핀
*/

/*------setup------*/
void setup()
{
  Serial.begin(115200);
  /*-----pinMode INPUT_PULLUP-----*/
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(ACTIVE_PIN, INPUT_PULLUP);

  /*-----pinMode OUTPUT-----*/
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);

  /*------DS18B20설정부------*/
  sensors.begin(); // DS18B20 센서 초기화
  sensors.setWaitForConversion(false); // 비동기식으로 온도 측정
  sensors.requestTemperatures(); // 온도 측정 요청

  /*------display설정부------*/
  if (!display.begin(SSD1306_I2C_ADDRESS, SSD1306_I2C_ADDRESS))
  {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  /*------Interrupt설정부------*/
  attachInterrupt(BUTTON_UP, Button_up, FALLING);
  attachInterrupt(BUTTON_DOWN, Button_down, FALLING);
  attachInterrupt(BUTTON_ENTER, Enter_func, FALLING);
  attachInterrupt(ACTIVE_PIN, Active, FALLING); //버튼 누를 시 Active 카운트가 늘어남 0, 1, 2에 모드 할당 후 구분

  /*------PWM설정부------*/
  pinMode(PWM_PIN, OUTPUT); // PWM 핀 설정
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION); // PWM 설정
  ledcAttachPin(PWM_PIN, PWM_CHANNEL); // PWM 핀과 채널 연결
  
  ledcWrite(PWM_CHANNEL, pwmValue); // 초기 PWM 값 설정
}

/*------loop-------*/
void loop()
{ 
  
  /*-----온도 측정부-----*/
  if(sensors.isConversionComplete()){
    float temperatureC = sensors.getTempCByIndex(0); // 측정온도 저장
    sensors.requestTemperatures(); // 다음 측정을 위해 온도 요청
  }

  /*-----온도 센서 오류 발생 시 오류 메세지 출력-----*/
  if (temperatureC == DEVICE_DISCONNECTED_C)
  {
    Serial.println("Error: Sensor not found!");
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Sensor Error");
    display.display();
    delay(1000); // 1초 대기 후 다시 시도
    return;      // 에러 발생 시 루프 종료
  }

  /*----------동작 모드 설정부----------*/
  /*
  동작모드 설정 목표
  - 유지 모드 : 설정온도와 현재온도가 같을 때, 가열/냉각 모드 모두 OFF
  - 가열 모드 : 설정온도가 현재온도보다 높을 때, 가열 모드 ON, 냉각 모드 OFF
  - 냉각 모드 : 설정온도가 현재온도보다 낮을 때, 가열 모드 OFF, 냉각 모드 ON
  버튼 하나로 유지 ; 가열 냉각 ; 종료 설정 가능 -> 0, 1, 2로 설정 0 : 정지, 1 : 가열 or 냉각, 2 : 유지
  - 버튼을 눌렀을 때, 설정온도 상승/하강, 모드 변경 가능
  - 가열, 냉각 모드했을 때 이를 스스로 냉각과 가열을 구분
  - 유지모드시 온도 유지지
  - 설정온도는 15도에서 85도까지 설정 가능
  - 설정온도는 1도 단위로 설정 가능
  - 모드 oled에 표시
  - 설정온도 oled에 표시
  - 현재온도 oled에 표시
  - 버튼 기능 구현
  - 버튼1 : 설정온도 상승
  - 버튼2 : 설정온도 하강
  - 버튼3 : 모드 변경 (정지, 가열/냉각, 유지)
  - 버튼4 : 활성화 (가열/냉각 모드에 따라 설정)
  */
 /*-----시스템 개요-----*/
 /*
  - 시스템은 DS18B20 온도 센서를 사용하여 현재 온도를 측정하고, 설정된 온도에 따라 가열 또는 냉각 모드를 자동으로 전환합니다.
  - 사용자는 OLED 디스플레이를 통해 현재 온도와 설정 온도를 확인할 수 있으며, 버튼을 통해 설정 온도를 조절할 수 있습니다.
  - 시스템은 정지 모드, 활성화 모드, 유지 모드로 작동하며, 각 모드에 따라 PWM 신호를 조절하여 열전소자를 제어합니다.
  - 정지 모드에서는 PWM 신호가 0으로 설정되어 열전소자가 작동하지 않으며, 활성화 모드에서는 현재 온도에 따라 PWM 신호가 조절되어 가열 또는 냉각이 이루어집니다.
  - 유지 모드에서는 설정 온도와 현재 온도가 같을 때 PWM 신호가 0으로 설정되어 열전소자가 작동하지 않으며, 설정 온도와 현재 온도가 다를 때는 가열 또는 냉각 모드로 전환되어 PWM 신호가 조절됩니다.
  - 사용자는 버튼을 눌러 설정 온도를 조절할 수 있으며, 설정 온도는 15도에서 85도까지 1도 단위로 조절 가능합니다.
  - 3번 버튼을 누르면 시스템 설정 변경 모드로 들어가며 이때 사용자는 원하는 모드를 4번 버튼을 눌러 선택할 수 있습니다. 이는 정지 모드, 활성화 모드, 유지 모드로 나뉘며 각 모드는 순서대로 display를 통해 사용자에게 표시됩니다.
  - 활성화 모드에 들어가면 사용자는 1번 버튼과 2번 버튼을 통해 설정 온도를 조절할 수 있으며, 설정 온도에 따라 가열 또는 냉각 모드로 전환됩니다.
  - 설정 온도는 OLED 디스플레이에 표시되며, 현재 온도와 함께 확인할 수 있습니다.
  - 활성화 모드에서 설정을 마치면 4번 버튼을 눌러 시스템을 활성화할 수 있으며, 이때 가열 또는 냉각 모드로 전환됩니다.
  - 시스템은 설정 온도와 현재 온도를 비교하여 가열 또는 냉각 모드를 자동으로 전환하며, 이를 통해 사용자는 원하는 온도를 유지할 수 있습니다.
  - 시스템은 사용자가 설정한 온도에 도달하면 PWM 신호를 0으로 설정하여 열전소자가 작동하지 않도록 합니다.
  - 실행 시 모드를 선택할 수 있도록 합니다.
 */

  /*-----------PWM 설정부 / 동작----------*/
  static float lastTemperature = 0; // 마지막 온도 저장 변수

  /*-----User Mode Setting I/O-----*/
  if(control_mode == STOP_MODE) // 정지 모드일 때
  { 
    while (control_mode != STOP_MODE)
    {
      if(user_control_mode == STOP_MODE) {
        mainDisplayPrint();
        delay(1000); // 1초 대기
        display.clearDisplay(); // display 초기화
        DisplayPrintNStop();
        delay(1000); // 1초 대기
        display.clearDisplay(); // display 초기화
      }
      else if(user_control_mode == ACTIVE_MODE) {
        mainDisplayPrint();
        delay(1000); // 1초 대기
        display.clearDisplay(); // display 초기화
        DisplayPrintNActive();
        delay(1000); // 1초 대기
        display.clearDisplay(); // display 초기화
      }
      else if(user_control_mode == KEEP_TEMPERATURE_MODE) {
        mainDisplayPrint();
        delay(1000); // 1초 대기
        display.clearDisplay(); // display 초기화
        DisplayPrintNKeep();
        delay(1000); // 1초 대기
        display.clearDisplay(); // display 초기화
      }
    }
  }

  //STOP_MODE 동작
  if (control_mode == STOP_MODE) // 정지 모드일 때
  {
    changeControlMode(STOP_MODE); // 정지 모드로 변경
    pwmValue = 0; // PWM 값 0으로 설정
    ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
    display.clearDisplay();
  }
  //ACTIVE_MODE 동작
  if (control_mode == ACTIVE_MODE) { 
    pwmValue = map(temperatureC, lastTemperature, setTemperature, 0, 255); // 온도에 따라 PWM 값 설정
    if (temperatureC > setTemperature) // 현재 온도가 설정 온도보다 높을 때
    {
      pwmValue = map(temperatureC, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      changeControlMode(HEATER_MODE); // 가열 모드로 변경
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
      displayPrint("Heating...");
    }
    else if (temperatureC < setTemperature) // 현재 온도가 설정 온도보다 낮을 때
    {
      pwmValue = map(SYSTEM_MAX_TEMPERATURE - temperatureC, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      changeControlMode(COOLER_MODE); // 냉각 모드로 변경
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
      displayPrint("Cooling...");
    }
    else if(temperatureC == setTemperature) // 현재 온도가 설정 온도와 같을 때
    {
      pwmValue = 0; // PWM 값 0으로 설정
      control_mode = KEEP_TEMPERATURE_MODE; // 유지 모드로 변경
      lastTemperature = temperatureC; // 마지막 온도 업데이트
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
    }
  }
  //KEEP_TEMPERATURE_MODE 동작
  if (control_mode == KEEP_TEMPERATURE_MODE) { // 유지 모드일 때
    if (temperatureC > setTemperature) // 현재 온도가 설정 온도보다 높을 때
    {
      pwmValue = map(lastTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      changeControlMode(HEATER_MODE); // 가열 모드로 변경
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
    }
    else if (temperatureC < setTemperature) // 현재 온도가 설정 온도보다 낮을 때
    {
      pwmValue = map(SYSTEM_MAX_TEMPERATURE - lastTemperature, SYSTEM_MIN_TEMPERATURE, SYSTEM_MAX_TEMPERATURE, 0, 255); // PWM 값 설정
      changeControlMode(COOLER_MODE); // 냉각 모드로 변경
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
    }
    else if(temperatureC == setTemperature) // 현재 온도가 설정 온도와 같을 때
    {
      pwmValue = 0; // PWM 값 0으로 설정
      ledcWrite(PWM_CHANNEL, pwmValue); // PWM 값 출력
    }
    displayPrint("Keeping temp...");
  }

  delay(100); // 100ms 대기
}
/*-----loop 종료-----*/

/*-----display 출력 간소화 함수-----*/
void displayPrint(const char *text)
{
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(text);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("Temperature: ");
  display.print(setTemperature);
  display.print(" C / ");
  display.print(temperatureC);
  display.println(" C");
  display.display();
}

/*-----Main Display Print-----*/
void mainDisplayPrint()
{
  display.clearDisplay(); // display 초기화
  display.setTextSize(2); // 텍스트 크기 설정
  display.setCursor(0, 0); // 커서 위치 설정
  display.println("Set Mode");
  display.setTextSize(1); // 텍스트 크기 설정
  display.setCursor(0, 20); // 커서 위치 설정
  display.println("1. EXIT ");
  display.println("2. ACTIVE ");
  display.println("3. KEEP Temperature ");
  display.setCursor(0, 40); // 커서 위치 설정
  display.print(temperatureC);
  display.print(" C ");
}

void DisplayPrintNStop()
{
  display.clearDisplay(); // display 초기화
  display.setTextSize(2); // 텍스트 크기 설정
  display.setCursor(0, 0); // 커서 위치 설정
  display.println("Set Mode");
  display.setTextSize(1); // 텍스트 크기 설정
  display.setCursor(0, 20); // 커서 위치 설정
  display.println(" ");
  display.println("2. ACTIVE ");
  display.println("3. KEEP Temperature ");
  display.setCursor(0, 40); // 커서 위치 설정
  display.print(temperatureC);
  display.print(" C ");
}

void DisplayPrintNActive()
{
  display.clearDisplay(); // display 초기화
  display.setTextSize(2); // 텍스트 크기 설정
  display.setCursor(0, 0); // 커서 위치 설정
  display.println("Set Mode");
  display.setTextSize(1); // 텍스트 크기 설정
  display.setCursor(0, 20); // 커서 위치 설정
  display.println("1. EXIT ");
  display.println(" ");
  display.println("3. KEEP Temperature ");
  display.setCursor(0, 40); // 커서 위치 설정
  display.print(temperatureC);
  display.print(" C ");
}

void DisplayPrintNKeep()
{
  display.clearDisplay(); // display 초기화
  display.setTextSize(2); // 텍스트 크기 설정
  display.setCursor(0, 0); // 커서 위치 설정
  display.println("Set Mode");
  display.setTextSize(1); // 텍스트 크기 설정
  display.setCursor(0, 20); // 커서 위치 설정
  display.println("1. EXIT ");
  display.println("2. ACTIVE ");
  display.println(" ");
  display.setCursor(0, 40); // 커서 위치 설정
  display.print(temperatureC);
  display.print(" C ");
}

/*-----Interrupt 함수 정의 부분-----*/
// 값을 감소시키는 버튼
void IRAM_ATTR Button_down() //
{ 
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeDown > debounceDelay)
  {
    lastDebounceTimeDown = currentTime;
    if (control_mode == STOP_MODE) {
      user_control_mode -= 1;
      if (user_control_mode < 0) {
        user_control_mode = 2; // 상태 초기화
      } 
    }
    else if (control_mode == ACTIVE_MODE) {
      if (setTemperature > MIN_TEMPERATURE)
      {
        setTemperature -= 1;
        Serial.println("temperature down");
      }
    }
  }
}
// 값을 증가시키는 버튼
void IRAM_ATTR Button_up()
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeUp > debounceDelay)
  {
    lastDebounceTimeUp = currentTime;
    if (control_mode == STOP_MODE) {
      user_control_mode += 1;
      if (user_control_mode > 2) {
        user_control_mode = 0; // 상태 초기화
      } 
    }
    else if (control_mode == ACTIVE_MODE) {
      if (setTemperature < MAX_TEMPERATURE)
      {
        setTemperature += 1;
        Serial.println("temperature up");
      }
    }
  }
}

void IRAM_ATTR Active()
{
  unsigned long currentTime = millis();
  if (currentTime - lastActiveTime > debounceDelay)
  {
    lastDebounceTimeUp = currentTime;
    activecontrol += 1; // 활성화 핀 상태 반전
    if (activecontrol > 2)
    {
      activecontrol = 0; // 상태 초기화
    }
    if(activecontrol == 0)
    {
      control_mode = STOP_MODE; // 정지 모드
    }
    else if(activecontrol == 1)
    {
      control_mode = ACTIVE_MODE; // 활성화 모드
    }
    else if(activecontrol == 2)
    {
      control_mode = KEEP_TEMPERATURE_MODE; // 유지 모드
    }
  }
}

void IRAM_ATTR Enter_func()
{
  unsigned long currentTime = millis();
  if (currentTime - lastDebounceTimeMode > debounceDelay)
  {
    lastDebounceTimeMode = currentTime;
    if (control_mode == STOP_MODE) {
      control_mode = user_control_mode;
    }
  }
}

/*------모드 변경 함수------*/
void changeControlMode(char control_device_mode)
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
  else if (control_device_mode == STOP_MODE)
  {
    Serial.println("Stop Mode");
    digitalWrite(HEATER_PIN, LOW);  // 가열 모드 핀 LOW
    digitalWrite(COOLER_PIN, LOW);  // 냉각 모드 핀 LOW
  }
  else
   return; // 유지 모드일 경우 아무 동작도 하지 않음
}