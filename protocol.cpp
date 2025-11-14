#include <Arduino.h>
#include <ArduinoJson.h>
#include "Protocol.h"  // 자신의 헤더
#include "config.h"    // 핀맵
#include "state.h"     // 전역 변수(current, state) 사용

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

// ===== 핀모드 설정 (Setting 시 호출) =====
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

// ===== 설정 적용 및 검증 (Setting 시 호출) =====
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
  current = s; // 전역 변수 'current'에 적용
}

void printMappingSummary(const Setting& s) {
  // (디버깅용 매핑 요약 함수)
  // ...
}

// =======================================================
// ===               ★ 제어 함수 (API 2.x)            ===
// =======================================================

bool handleCupCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.cup) { printError("invalid cup control num"); return false; }
  uint8_t idx = control - 1; 

  if (strcmp(func, "startdispense") == 0) {
    digitalWrite(CUP_MOTOR_OUT[idx], HIGH); // ON
    printOk("cup startdispense");
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(CUP_MOTOR_OUT[idx], LOW); // OFF
    printOk("cup stopdispense");
  } else { printError("unknown cup function"); }
  return true;
}

bool handleRamenCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.ramen) { printError("invalid ramen control num"); return false; }
  uint8_t idx = control - 1;
  printOk("start handle ramen");

  if (strcmp(func, "startdispense") == 0) {
    printOk("current index : ", RAMEN_EJ_FWD_OUT[idx]);
    digitalWrite(RAMEN_EJ_FWD_OUT[idx], HIGH); // ON
    printOk("ramen startdispense");
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(RAMEN_EJ_FWD_OUT[idx], LOW); // OFF
    printOk("ramen stopdispense");
  } else { printError("unknown ramen function"); }
  return true;
}

bool handlePowderCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.powder) { printError("invalid powder control num"); return false; }
  uint8_t idx = control - 1;

  if (strcmp(func, "startdispense") == 0) {
    digitalWrite(POWDER_MOTOR_OUT[idx], HIGH); // ON
    printOk("powder startdispense");
  } else if (strcmp(func, "stopdispense") == 0) {
    digitalWrite(POWDER_MOTOR_OUT[idx], LOW); // OFF
    printOk("powder stopdispense");
  } else { printError("unknown powder function"); }
  return true;
}

bool handleCookerCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.cooker) { printError("invalid cooker control num"); return false; }
  uint8_t idx = control - 1;

  if (strcmp(func, "startcook") == 0) {
    int water = doc["water"]; 
    int timer = doc["timer"]; 
    if (idx < 2) { // 1, 2번 장비만 출력
      digitalWrite(COOKER_WTR_SIG[idx], HIGH); // ON
      digitalWrite(COOKER_IND_SIG[idx], HIGH); // ON
    }
    printOk("cooker startcook");
  } else if (strcmp(func, "stopcook") == 0) {
    if (idx < 2) {
      digitalWrite(COOKER_WTR_SIG[idx], LOW); // OFF
      digitalWrite(COOKER_IND_SIG[idx], LOW); // OFF
    }
    printOk("cooker stopcook");
  } else { printError("unknown cooker function"); }
  return true;
}

bool handleOutletCommand(const JsonDocument& doc) {
  int control = doc["control"] | 0;
  const char* func = doc["function"] | "";
  if (control <= 0 || control > current.outlet) { printError("invalid outlet control num"); return false; }
  uint8_t idx = control - 1;

  if (strcmp(func, "opendoor") == 0) {
    digitalWrite(OUTLET_FWD_OUT[idx], HIGH); // ON
    digitalWrite(OUTLET_REV_OUT[idx], LOW);  // OFF
    printOk("outlet opendoor");
  } else if (strcmp(func, "closedoor") == 0) {
    digitalWrite(OUTLET_FWD_OUT[idx], LOW);  // OFF
    digitalWrite(OUTLET_REV_OUT[idx], HIGH); // ON
    printOk("outlet closedoor");
  } else if (strcmp(func, "stopoutlet") == 0) {
    digitalWrite(OUTLET_FWD_OUT[idx], LOW); // OFF
    digitalWrite(OUTLET_REV_OUT[idx], LOW); // OFF
    printOk("outlet stopoutlet");
  } else { printError("unknown outlet function"); }
  return true;
}

// =======================================================
// ===       ★ 설정 함수 (Setting / Query)            ===
// =======================================================

bool handleSettingJson(const JsonDocument& doc) {
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
  // printMappingSummary(next); // 디버깅 필요시 주석 해제
  return true;
}

// =======================================================
// ===            ★ 메인 JSON 파서/분배기             ===
// =======================================================

bool parseAndDispatch(const char* json) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) { printError("json parse fail"); return false; }

  const char* dev = doc["device"] | "";

  // 1. 설정 또는 질의 명령
  if (strcmp(dev, "setting") == 0) {
    return handleSettingJson(doc);
  } else if (strcmp(dev, "query") == 0) {
    replyCurrentSetting(current);
    return true;
  }
  
  // 2. 제어 명령
  else if (strcmp(dev, "cup") == 0) {
    return handleCupCommand(doc);
  } else if (strcmp(dev, "ramen") == 0) {
    return handleRamenCommand(doc);
  } else if (strcmp(dev, "powder") == 0) {
    return handlePowderCommand(doc);
  } else if (strcmp(dev, "cooker") == 0) {
    return handleCookerCommand(doc);
  } else if (strcmp(dev, "outlet") == 0) {
    return handleOutletCommand(doc);
  }
  
  // 3. 알 수 없는 명령
  else {
    printError("unsupported device field");
    return false;
  }
}