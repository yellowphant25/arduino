#include <Arduino.h>
#include <ArduinoJson.h>

// 모듈 헤더파일 포함
#include "config.h"     // 핀맵, 상수
#include "state.h"      // Setting/State 구조체, 전역변수 선언
#include "protocol.h"   // 수신 명령(API 2.x) 처리
#include "reporting.h"  // 상태 보고(API 1.x) 처리

// ===== 전역 변수 정의 =====
Setting current;
State state;
String rx;
unsigned long lastPublishMs = 0;

// ===== 엔코더 관련 설정 =====
const int ENCODER_A_PIN = 2;
const int ENCODER_B_PIN = 3;

const int PPR = 100;           
const int CPR = PPR * 4;      

volatile long encoderCount = 0;
volatile int  direction = 0;   // +1: 정회전, -1: 역회전

unsigned long lastEncoderReportTime = 0;
long lastCount = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }

// Due 보드 ADC 해상도 10비트(0-1023)로 통일
#ifdef ARDUINO_ARCH_SAM
  analogReadResolution(10);
#endif

  // [신규] 출입문 센서 핀 초기화 (항상 활성화로 가정)
  pinMode(DOOR_SENSOR1_PIN, INPUT);  // (필요시 INPUT_PULLUP)
  pinMode(DOOR_SENSOR2_PIN, INPUT);

  Serial.println(F("{\"boot\":\"ready\",\"hint\":\"send {\\\"device\\\":\\\"setting\\\",...} or {\\\"device\\\":\\\"query\\\"}\"}"));
  lastPublishMs = millis();

  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);

  // A, B 둘 다 인터럽트 사용 (정밀도 높게)
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), handleEncoderA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B_PIN), handleEncoderB, CHANGE);
}

void loop() {
  if (current.cup > 0) {
    checkCupDispense();  // (1번 장비 컵 멈춤 감시)
  }
  if (current.ramen > 0) {
    checkRamenRise();   // (1번 장비 면 상승 멈춤 감시)
    checkRamenInit();   // (1번 장비 면 하강 멈춤 감시)
    checkRamenEject();  // (1번 장비 면 배출 상태머신)
  }
  if (current.powder > 0) {
    checkPowderDispense();  // (1번 장비 스프 타이머 감시)
  }
  if (current.outlet > 0) {
    checkOutlet();  // (모든 배출구 멈 춤 감시)
  }

  // Serial.print("면 배출 상한 센서 : ");
  // Serial.println(digitalRead(8));
  // Serial.print("면 배출 하한 센서 : ");
  // Serial.println(digitalRead(9));
  // Serial.print("면 상승 상한 센서 : ");
  // Serial.println(digitalRead(10));
  // Serial.print("면 상승 하한 센서 :");
  // Serial.println(digitalRead(11));

  // ================================================
  // 2. [실시간] JSON 명령 수신
  // ================================================
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (rx.length() > 0) {
        parseAndDispatch(rx.c_str());
        rx = "";
      }
    } else {
      rx += c;
    }
  }

  unsigned long now = millis();
  if (now - lastPublishMs >= PUBLISH_INTERVAL_MS) {
    lastPublishMs = now;

    // 현재 설정된 장비가 있을 때만 센서 읽기 및 전송
    if (current.cup > 0 || current.ramen > 0 || current.powder > 0 || current.cooker > 0 || current.outlet > 0) {
      readAllSensors();     // Reporting.cpp 에 정의됨
      publishStateJson();
    } else {
      // setting이 안된 상태에서도 전송
      state.door_sensor1 = digitalRead(DOOR_SENSOR1_PIN);
      state.door_sensor2 = digitalRead(DOOR_SENSOR2_PIN);

      StaticJsonDocument<128> doorDoc;
      doorDoc["device"] = "door";
      doorDoc["sensor1"] = state.door_sensor1;
      doorDoc["sensor2"] = state.door_sensor2;
      serializeJson(doorDoc, Serial);
      Serial.println();
    }
  }
}