#include <Arduino.h>
#include <ArduinoJson.h>
#include "Reporting.h" // 자신
#include "config.h"    // 핀맵
#include "state.h"     // 전역 변수(current, state) 사용

// 6-1. 모든 활성 센서 값 읽기 (state 변수에 저장)
void readAllSensors() {
  // 1-1. Cup
  for (uint8_t i = 0; i < current.cup; i++) {
    state.cup_amp[i] = analogRead(CUP_CURR_AIN[i]);
    state.cup_stock[i] = digitalRead(CUP_STOCK_IN[i]);
    state.cup_dispense[i] = digitalRead(CUP_ROT_IN[i]);
  }
  
  // 1-2. Ramen
  for (uint8_t i = 0; i < current.ramen; i++) {
    state.ramen_amp[i] = analogRead(RAMEN_EJ_CURR_AIN[i]);
    state.ramen_stock[i] = digitalRead(RAMEN_PRESENT_IN[i]);
    // state.ramen_lift[i] = ... (엔코더 값 계산 로직 필요)
    // state.ramen_loadcell[i] = ... (로드셀 핀이 명세서에 없음)
  }

  // 1-3. Powder
  for (uint8_t i = 0; i < current.powder; i++) {
    state.powder_amp[i] = analogRead(POWDER_CURR_AIN[i]);
    state.powder_dispense[i] = (digitalRead(POWDER_MOTOR_OUT[i]) == LOW) ? 1 : 0; 
  }
  
  // 1-4. Cooker
  for (uint8_t i = 0; i < current.cooker; i++) {
    state.cooker_amp[i] = analogRead(COOKER_CURR_AIN[i]);
    // state.cooker_work[i] = ... (실제 동작 상태 로직 필요)
  }

  // 1-5. Outlet
  for (uint8_t i = 0; i < current.outlet; i++) {
    state.outlet_amp[i] = analogRead(OUTLET_CURR_AIN[i]);
    state.outlet_sonar[i] = analogRead(OUTLET_USONIC_AIN[i]);
    state.outlet_loadcell[i] = analogRead(OUTLET_LOAD_AIN[i]);
    // state.outlet_door[i] = ... (열림/닫힘/회전중(9) 상태 로직 필요)
  }

  // 1-6. Door (항상 활성)
  state.door_sensor1 = digitalRead(DOOR_SENSOR1_PIN);
  state.door_sensor2 = digitalRead(DOOR_SENSOR2_PIN);
}

// 6-2. 상태 JSON 생성 및 전송 (API 1.x)
void publishStateJson() {
  StaticJsonDocument<512> doc;

  // 1-1. Cup
  for (uint8_t i = 0; i < current.cup; i++) {
    doc.clear();
    doc["device"] = "cup";
    doc["control"] = i + 1;
    doc["amp"] = state.cup_amp[i];
    doc["stock"] = state.cup_stock[i];
    doc["dispense"] = state.cup_dispense[i];
    serializeJson(doc, Serial);
    Serial.println();
  }
  
  // 1-2. Ramen
  for (uint8_t i = 0; i < current.ramen; i++) {
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

  // 1-3. Powder
  for (uint8_t i = 0; i < current.powder; i++) {
    doc.clear();
    doc["device"] = "powder";
    doc["control"] = i + 1;
    doc["amp"] = state.powder_amp[i];
    doc["dispense"] = state.powder_dispense[i];
    serializeJson(doc, Serial);
    Serial.println();
  }
  
  // 1-4. Cooker
  for (uint8_t i = 0; i < current.cooker; i++) {
    doc.clear();
    doc["device"] = "cooker";
    doc["control"] = i + 1;
    doc["amp"] = state.cooker_amp[i];
    doc["work"] = state.cooker_work[i];
    serializeJson(doc, Serial);
    Serial.println();
  }

  // 1-5. Outlet
  for (uint8_t i = 0; i < current.outlet; i++) {
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

  // 1-6. Door
  doc.clear();
  doc["device"] = "door";
  doc["sensor1"] = state.door_sensor1;
  doc["sensor2"] = state.door_sensor2;
  serializeJson(doc, Serial);
  Serial.println();
}