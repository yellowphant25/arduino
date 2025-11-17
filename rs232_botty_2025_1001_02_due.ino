/*
  DUE 조리기 제어 - 전체 API 규격서 통합 구현 (파일 분리)
  - 메인 스케치 파일
*/

#include <Arduino.h>
#include <ArduinoJson.h>

// 모듈 헤더파일 포함
#include "config.h"    // 핀맵, 상수
#include "state.h"     // Setting/State 구조체, 전역변수 선언
#include "Protocol.h"  // 수신 명령(API 2.x) 처리
#include "Reporting.h" // 상태 보고(API 1.x) 처리

// ===== 전역 변수 정의 =====
Setting current; // 현재 적용된 '역할' 설정
State state;     // 모든 센서의 현재 값을 저장
String rx;       // 시리얼 수신 버퍼
unsigned long lastPublishMs = 0;
unsigned long encoder1 = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {;}

  // Due 보드 ADC 해상도 10비트(0-1023)로 통일
  #ifdef ARDUINO_ARCH_SAM
    analogReadResolution(10);
  #endif
  
  // [신규] 출입문 센서 핀 초기화 (항상 활성화로 가정)
  pinMode(DOOR_SENSOR1_PIN, INPUT); // (필요시 INPUT_PULLUP)
  pinMode(DOOR_SENSOR2_PIN, INPUT);

  Serial.println(F("{\"boot\":\"ready\",\"hint\":\"send {\\\"device\\\":\\\"setting\\\",...} or {\\\"device\\\":\\\"query\\\"}\"}"));
  lastPublishMs = millis();
}

void loop() {
  // JSON 명령 수신 처리
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (rx.length() > 0) {
        parseAndDispatch(rx.c_str()); // Protocol.cpp 에 정의됨
        rx = "";
      }
    } else {
      rx += c;
    }
  }

  // 0.1초마다 상태 리포팅
  // unsigned long now = millis();
  // if (now - lastPublishMs >= PUBLISH_INTERVAL_MS) {
  //   lastPublishMs = now;
    
  //   // 현재 설정된 장비가 있을 때만 센서 읽기 및 전송
  //   if (current.cup > 0 || current.ramen > 0 || current.powder > 0 || current.cooker > 0 || current.outlet > 0) {
  //     readAllSensors();     // Reporting.cpp 에 정의됨
  //     publishStateJson();   // Reporting.cpp 에 정의됨
  //   } else {
  //     // setting이 안된 상태에서도 전송
  //     state.door_sensor1 = digitalRead(DOOR_SENSOR1_PIN);
  //     state.door_sensor2 = digitalRead(DOOR_SENSOR2_PIN);
      
  //     StaticJsonDocument<128> doorDoc;
  //     doorDoc["device"] = "door";
  //     doorDoc["sensor1"] = state.door_sensor1;
  //     doorDoc["sensor2"] = state.door_sensor2;
  //     serializeJson(doorDoc, Serial);
  //     Serial.println();
  //   }
  // }


}