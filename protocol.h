#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "state.h" // 'Setting' 구조체를 사용하기 위해 포함

// =======================================================
// === 1. 메인 파서 및 설정 함수
// =======================================================

// 메인 JSON 파서
bool parseAndDispatch(const char* json);

// 설정 적용 함수 (Setting 시 호출)
void applySetting(const Setting& s);
void replyCurrentSetting(const Setting& s);
bool validateRules(const Setting& s, String& why);

// 핀모드 설정 함수 (applySetting 내부에서 호출)
void setupCup(uint8_t n);
void setupRamen(uint8_t n);
void setupPowder(uint8_t n);
void setupOutlet(uint8_t n);
void setupCooker(uint8_t n);


// =======================================================
// === 2. 비동기 "시작" 함수 (JSON 핸들러가 호출)
// =======================================================

// --- Cup (1번 장비 전용) ---
void startCupDispense();

// --- Ramen (1번 장비 전용) ---
void startRamenRise();
void startRamenInit();
void startRamenEject();

// --- Powder (1번 장비 전용) ---
void startPowderDispense();

// --- Outlet (모든 장비) ---
void startOutletOpen(int pinIdx);
void startOutletClose(int pinIdx);


// =======================================================
// === 3. 비동기 "감시" 함수 (Main.ino의 loop()가 호출)
// =======================================================

// --- Cup (1번 장비 전용) ---
void checkCupDispense();

// --- Ramen (1번 장비 전용) ---
void checkRamenRise();
void checkRamenInit();
void checkRamenEject();

// --- Powder (1번 장비 전용) ---
void checkPowderDispense();

// --- Outlet (모든 장비) ---
void checkOutlet(); // (내부에서 모든 Outlet을 검사)

// 🔴 [삭제됨]
// finishOutletOpen() 및 finishOutletClose() 함수는
// checkOutlet() 함수로 통합되었으므로 헤더에서 제거합니다.

#endif // PROTOCOL_H