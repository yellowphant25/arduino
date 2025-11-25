#include <Arduino.h>
#include <ArduinoJson.h>
#include "reporting.h"
#include "config.h" 
#include "state.h"

void readAllSensors() {
  uint8_t i;

  for (i = 0; i < current.cup; i++) {
    state.cup_amp[i] = analogRead(CUP_CURR_AIN[i]);
    state.cup_stock[i] = digitalRead(CUP_STOCK_IN[i]);
    state.cup_dispense[i] = digitalRead(CUP_ROT_IN[i]);
  }

  for (i = 0; i < current.ramen; i++) { 
    state.ramen_amp[i] = analogRead(RAMEN_EJ_CURR_AIN[i]);
    state.ramen_stock[i] = digitalRead(RAMEN_PRESENT_IN[i]);
    // state.ramen_lift[i] = ... (엔코더 값 계산 로직 필요)
  }

  for (i = 0; i < current.powder; i++) {
    state.powder_amp[i] = analogRead(POWDER_CURR_AIN[i]);
    state.powder_dispense[i] = (digitalRead(POWDER_MOTOR_OUT[i]) == LOW) ? 1 : 0; 
  }

  for (i = 0; i < current.cooker; i++) {
    state.cooker_amp[i] = analogRead(COOKER_CURR_AIN[i]);
    // state.cooker_work[i] = ...
  }

  for (i = 0; i < current.outlet; i++) {
    state.outlet_amp[i] = analogRead(OUTLET_CURR_AIN[i]);
    state.outlet_sonar[i] = analogRead(OUTLET_USONIC_AIN[i]);
    state.outlet_loadcell[i] = analogRead(OUTLET_LOAD_AIN[i]);
    // state.outlet_door[i] = ...
  }

  state.door_sensor1 = digitalRead(DOOR_SENSOR1_PIN);
  state.door_sensor2 = digitalRead(DOOR_SENSOR2_PIN);
}


void publishStateJson() {
  StaticJsonDocument<512> doc;
  uint8_t i;

  for (i = 0; i < current.cup; i++) {
    doc.clear();
    doc["device"] = "cup";
    doc["control"] = i + 1; // 1부터 Role Number까지 보고
    doc["amp"] = state.cup_amp[i];
    doc["stock"] = state.cup_stock[i];
    doc["dispense"] = state.cup_dispense[i];
    serializeJson(doc, Serial);
    Serial.println();
  }

  for (i = 0; i < current.ramen; i++) { 
    doc.clear();
    doc["device"] = "ramen";
    doc["control"] = i + 1;
    doc["amp"] = state.ramen_amp[i];
    doc["stock"] = state.ramen_stock[i];
    doc["lift"] = state.ramen_lift[i];
    doc["loadcell"] = state.ramen_loadcell[i];
    serializeJson(doc, Serial);
    Serial.println();
  }

  for (i = 0; i < current.powder; i++) {
    doc.clear();
    doc["device"] = "powder";
    doc["control"] = i + 1;
    doc["amp"] = state.powder_amp[i];
    doc["dispense"] = state.powder_dispense[i];
    serializeJson(doc, Serial);
    Serial.println();
  }

  for (i = 0; i < current.cooker; i++) {
    doc.clear();
    doc["device"] = "cooker";
    doc["control"] = i + 1;
    doc["amp"] = state.cooker_amp[i];
    doc["work"] = state.cooker_work[i];
    serializeJson(doc, Serial);
    Serial.println();
  }

  for (i = 0; i < current.outlet; i++) {
    doc.clear();
    doc["device"] = "outlet";
    doc["control"] = i + 1;
    doc["amp"] = state.outlet_amp[i];
    doc["door"] = state.outlet_door[i];
    doc["sonar"] = state.outlet_sonar[i];
    doc["loadcell"] = state.outlet_loadcell[i];
    serializeJson(doc, Serial);
    Serial.println();
  }

  doc.clear();
  doc["device"] = "door";
  doc["sensor1"] = state.door_sensor1;
  doc["sensor2"] = state.door_sensor2;
  serializeJson(doc, Serial);
  Serial.println();
}

void checkVolt() {
  int v = analogRead(A3);
  
  Serial.print("current vol : ");
  Serial.println(v);
}

// 엔코더 상태 출력 함수 (100ms 마다 실행)
void reportEncoderDebug(unsigned long currentMillis) {
  if (currentMillis - lastEncoderReportTime >= 100) {
    
    noInterrupts();
    long countCopy = encoderCount;
    int dirCopy = direction;
    interrupts();

    // 시간 차이 계산 (초 단위)
    float dt = (currentMillis - lastEncoderReportTime) / 1000.0;
    
    // 카운트 차이 및 속도 계산
    long dCount = countCopy - lastCount;
    float revPerSec = (float)dCount / (float)CPR / dt;
    float rpm = revPerSec * 60.0;
    
    // 각도 계산 (도 단위)
    float angleDeg = (float)countCopy * 360.0 / (float)CPR;

    // 상태 업데이트
    lastEncoderReportTime = currentMillis;
    lastCount = countCopy;

    // 시리얼 출력
    Serial.print("[Encoder] Count: ");
    Serial.print(countCopy);
    Serial.print(" | Angle: ");
    Serial.print(angleDeg, 1);
    Serial.print(" deg | Dir: ");
    Serial.print((dirCopy >= 0) ? "CW" : "CCW");
    Serial.print(" | RPM: ");
    Serial.println(rpm, 1);
  }
}

void handleEncoderA() {
  bool A = digitalRead(ENCODER_A_PIN);
  bool B = digitalRead(ENCODER_B_PIN);

  if (A == B) {
    encoderCount++;  // CW
    direction = +1;
  } else {
    encoderCount--;  // CCW
    direction = -1;
  }
}

// B 채널 인터럽트 서비스 루틴
void handleEncoderB() {
  bool A = digitalRead(ENCODER_A_PIN);
  bool B = digitalRead(ENCODER_B_PIN);

  if (A != B) {
    encoderCount++;  // CW
    direction = +1;
  } else {
    encoderCount--;  // CCW
    direction = -1;
  }
} 