#include <Arduino.h>
#include <ArduinoJson.h>
#include "Protocol.h"  // 자신
#include "config.h"    // 핀맵
#include "state.h"     // 전역 변수(current) 사용

// ===== 유틸리티 함수 (응답) =====
void printError(const char* msg) {
  Serial.print(F("{\"ok\":false,\"error\":\""));
  Serial.print(msg);
  Serial.println(F("\"}"));
}

void printOk(const char* what) {
  Serial.print(F("{\"ok\":true,\"msg\":\""));
  Serial.print(what);
  Serial.println(F("\"}"));
}

void replyCurrentSetting(const Setting& s) {
  StaticJsonDocument<256> doc;
  doc["device"] = "setting";
  if (s.cup)     doc["cup"]    = s.cup;
  if (s.ramen)   doc["ramen"]  = s.ramen;
  if (s.powder)  doc["powder"] = s.powder;
  if (s.cooker)  doc["cooker"] = s.cooker;
  if (s.outlet)  doc["outlet"] = s.outlet;
  serializeJson(doc, Serial);
  Serial.println();
}

// ===== 핀모드 설정 (이전과 동일) =====
void setupCup(uint8_t n) {
  for (uint8_t i=0;i<n;i++){
    pinMode(CUP_MOTOR_OUT[i], OUTPUT);
    pinMode(CUP_ROT_IN[i],    INPUT);
    pinMode(CUP_DISP_IN[i],   INPUT);
    pinMode(CUP_STOCK_IN[i],  INPUT);
  }
}
void setupRamen(uint8_t n) {
  for (uint8_t i=0;i<n;i++){
    pinMode(RAMEN_UP_FWD_OUT[i], OUTPUT);
    pinMode(RAMEN_UP_REV_OUT[i], OUTPUT);
    pinMode(RAMEN_EJ_FWD_OUT[i], OUTPUT);
    pinMode(RAMEN_EJ_REV_OUT[i], OUTPUT);
    pinMode(RAMEN_EJ_TOP_IN[i],  INPUT);
    pinMode(RAMEN_EJ_BTM_IN[i],  INPUT);
    pinMode(RAMEN_UP_TOP_IN[i],  INPUT);
    pinMode(RAMEN_UP_BTM_IN[i],  INPUT);
    pinMode(RAMEN_PRESENT_IN[i], INPUT);
    pinMode(RAMEN_ENC_INT[i],    INPUT);
  }
}
void setupPowder(uint8_t n) {
  for (uint8_t i=0;i<n;i++){
    pinMode(POWDER_MOTOR_OUT[i], OUTPUT);
  }
}
void setupOutlet(uint8_t n) {
  for (uint8_t i=0;i<n;i++){
    pinMode(OUTLET_FWD_OUT[i], OUTPUT);
    pinMode(OUTLET_REV_OUT[i], OUTPUT);
    pinMode(OUTLET_OPEN_IN[i], INPUT);
    pinMode(OUTLET_CLOSE_IN[i], INPUT);
  }
}
void setupCooker(uint8_t n) {
  for (uint8_t i=0;i<n;i++){
    if (i < 2) {
      pinMode(COOKER_IND_SIG[i], OUTPUT);
      pinMode(COOKER_WTR_SIG[i], OUTPUT);
    } else {
      pinMode(COOKER_IND_SIG[i], INPUT);
      pinMode(COOKER_WTR_SIG[i], INPUT);
    }
  }
}

// ===== 설정 적용 및 검증 (이전과 동일) =====
bool validateRules(const Setting& s, String& why) {
  uint8_t nonzeroCnt = (s.cup?1:0) + (s.ramen?1:0) + (s.powder?1:0) + (s.cooker?1:0) + (s.outlet?1:0);
  if (s.cup     > MAX_CUP)    { why = "cup max=4"; return false; }
  if (s.ramen   > MAX_RAMEN)  { why = "ramen max=4"; return false; }
  if (s.powder  > MAX_POWDER) { why = "powder max=8"; return false; }
  if (s.cooker  > MAX_COOKER) { why = "cooker max=4"; return false; }
  if (s.outlet  > MAX_OUTLET) { why = "outlet max=4"; return false; }
  if (nonzeroCnt == 0) { why = "no device count set"; return false; }
  if (s.cup>0 && s.cooker>0) {
    if (s.ramen==0 && s.powder==0 && s.outlet==0) return true;
    why = "only cup+cooker can be combined";
    return false;
  }
  if (nonzeroCnt == 1) return true;
  why = "invalid combination (only cup+cooker together; others solo)";
  return false;
}

void applySetting(const Setting& s) {
  if (s.cup)    setupCup(s.cup);
  if (s.ramen)  setupRamen(s.ramen);
  if (s.powder) setupPowder(s.powder);
  if (s.outlet) setupOutlet(s.outlet);
  if (s.cooker) setupCooker(s.cooker);
  current = s;
}

void printMappingSummary(const Setting& s) {
  // (디버깅용 매핑 요약 함수... 생략)
}

// ===== 'setting' 명령 핸들러 =====
// [수정] JsonDocument -> JsonObject
bool handleSettingJson(JsonObject doc) { 
  Setting next;
  next.cup     = doc["cup"]    | 0;
  next.ramen   = doc["ramen"]  | 0;
  next.powder  = doc["powder"] | 0;
  next.cooker  = doc["cooker"] | 0;
  next.outlet  = doc["outlet"] | 0;

  String why;
  if (!validateRules(next, why)) {
    printError(why.c_str());
    return false;
  }

  applySetting(next);
  printOk("pins configured");
  // printMappingSummary(next);
  return true;
}

// ===== [신규] 단일 명령 처리기 =====
bool processSingleCommand(JsonObject obj) {
  const char* dev = obj["device"] | "";
  
  if (strcmp(dev, "setting") == 0) {
    return handleSettingJson(obj);
  } else if (strcmp(dev, "query") == 0) {
    replyCurrentSetting(current);
    return true;
  } else {
    // 'setting' / 'query' 외의 명령 (cup, ramen 등)
    // 요구사항: Echo
    if (strlen(dev) > 0) {
        // 객체를 다시 문자열로 직렬화하여 반송
        serializeJson(obj, Serial);
        Serial.println();
        return true;
    } else {
        printError("unsupported device field");
        return false;
    }
  }
}

// ===== [수정] 메인 파서 (배열/객체 분기) =====
bool parseAndDispatch(const char* json) {
  // JSON 배열 크기에 맞춰 용량 증가
  StaticJsonDocument<2048> doc; 
  
  DeserializationError err = deserializeJson(doc, json);
  if (err) { 
    printError("json parse fail"); 
    return false; 
  }

  // 1. JSON 배열('[{}, {}]')인가?
  if (doc.is<JsonArray>()) {
    JsonArray arr = doc.as<JsonArray>();
    bool all_success = true;
    
    // 배열을 순회하며 모든 명령을 개별적으로 처리
    for (JsonObject obj : arr) {
      if (!processSingleCommand(obj)) {
        all_success = false;
      }
    }
    return all_success;
    
  } 
  // 2. 단일 JSON 객체('{}')인가?
  else if (doc.is<JsonObject>()) {
    JsonObject obj = doc.as<JsonObject>();
    return processSingleCommand(obj);
  } 
  // 3. 둘 다 아닌 경우
  else {
    printError("root must be object or array");
    return false;
  }
}