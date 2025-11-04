#ifndef INITIALIZE_H
#define INITIALIZE_H

#include <Arduino.h>
#include <ArduinoJson.h>

#define ROLE_CUP    1
#define ROLE_RAMEN  2
#define ROLE_POWDER 3
#define ROLE_OUTLET 4
#define ROLE_COOKER 5

/**
 * @brief 수신된 JSON과 보드 역할(Role)에 따라 하드웨어를 초기화하는 메인 함수
 * @param role 이 보드의 역할 (예: ROLE_CUP, ROLE_RAMEN 등)
 * @param jsonSettings 서버에서 받은 JSON 설정 문자열
 */
void initializeArduinoRole(int role, String jsonSettings);

/**
 * @brief Cup(용기) 디스펜서 핀을 초기화합니다.
 * @param count 설정할 장비 수량 (JSON에서 파싱)
 */
void initCupRobot(int count);

/**
 * @brief Ramen(면) 디스펜서 핀을 초기화합니다.
 * @param count 설정할 장비 수량
 */
void initRamenRobot(int count);

/**
 * @brief Powder(분말) 디스펜서 핀을 초기화합니다.
 * @param count 설정할 장비 수량
 */
void initPowderRobot(int count);

/**
 * @brief Outlet(배출구) 핀을 초기화합니다.
 * @param count 설정할 장비 수량
 */
void initOutletRobot(int count);

/**
 * @brief Cooker(인덕션) 핀을 초기화합니다. (명세서에만 있고 스텁에 없어서 추가)
 * @param count 설정할 장비 수량
 */
void initCookerRobot(int count);


#endif // INITIALIZE_H