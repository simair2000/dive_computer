
#include "buhlmann.h"
#include <math.h>

/**
 * @brief 배열을 대기 상태로 초기화 (다이빙 모드 진입 시 1회 호출)
 */
void Init_Loadings(float current_loadings[]) {
    // 수면에서의 불활성 기체 압력 계산
    // 대기압 1bar, 수증기압 제외, 공기 중 질소 79% 가정
    float initial_n2_pressure = (1.0f - WATER_VAPOR_PRESSURE) * 0.79f;

    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
        current_loadings[i] = initial_n2_pressure; 
        // 보통 약 0.74 ~ 0.79 bar 사이의 값이 됩니다.
    }
}

/**
 * @brief 조직 내 질소 로딩값을 갱신하는 함수 (슈라이너 방정식 적용)
 * 
 * @param current_loadings (입출력) 갱신할 질소 압력 배열 [bar]
 * @param depth_meters     현재 수심 [m]
 * @param fraction_o2      산소 농도 (예: 공기=0.21, EAN32=0.32)
 * @param interval_sec     지난번 계산 후 경과 시간 [초] (보통 1.0초)
 */
void Update_N2_Loadings(float current_loadings[], float depth_meters, float fraction_o2, float interval_sec) {
    
    // 1. 현재 주변 압력 (Ambient Pressure) 계산 [bar]
    // 해수 기준: 10m 당 1bar 증가 + 수면 기압 1bar (정확히는 1.01325이나 편의상 1.0 사용)
    float P_amb = 1.0f + (depth_meters / 10.0f);

    // 2. 흡입 기체 내 질소 분압 (Inspired N2 Pressure) 계산
    // 폐포 내에서는 수증기압(0.0627 bar)만큼 기체 압력이 낮아짐을 고려 (생리학적 정밀도)
    // 질소 비율 = 1.0 - 산소 비율 (기타 기체 무시)
    float fraction_n2 = 1.0f - fraction_o2;
    float P_gas_n2 = (P_amb - WATER_VAPOR_PRESSURE) * fraction_n2;

    // 만약 수증기압 보정을 안 하려면: float P_gas_n2 = P_amb * fraction_n2;

    // 3. 16개 조직에 대해 루프를 돌며 슈라이너 방정식 적용
    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
        
        // 이전 상태의 조직 내 질소압
        float P_prev = current_loadings[i];

        // 슈라이너 방정식의 지수항 계산 (Haldane model)
        // 공식: P_new = P_prev + (P_gas - P_prev) * (1 - 2^(-time / half_life))
        // 주의: interval_sec는 초 단위, half_life는 분 단위이므로 단위를 맞춰야 함 ( / 60.0 )
        
        float time_minutes = interval_sec / 60.0f;
        float exponent = -time_minutes / N2_Half_Lives[i];
        float decay_factor = 1.0f - powf(2.0f, exponent);

        // 값 갱신
        current_loadings[i] = P_prev + (P_gas_n2 - P_prev) * decay_factor;
    }
}