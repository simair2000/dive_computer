
#include "buhlmann.h"
#include <math.h>

// ZHL-16C 질소 반감기 (단위: 분)
float N2_Half_Lives[NUM_COMPARTMENTS] = {
    4.0f, 5.0f, 8.0f, 12.5f, 18.5f, 27.0f, 38.3f, 54.3f, 
    77.0f, 109.0f, 146.0f, 187.0f, 239.0f, 305.0f, 390.0f, 635.0f
};

// a 계수
float A_Coefficients[NUM_COMPARTMENTS] = {
    1.2599f, 1.0000f, 0.8618f, 0.7562f, 0.6667f, 0.5600f, 0.4947f, 0.4500f,
    0.4187f, 0.3798f, 0.3497f, 0.3223f, 0.2850f, 0.2737f, 0.2523f, 0.2327f
};

// b 계수
float B_Coefficients[NUM_COMPARTMENTS] = {
    0.5050f, 0.5533f, 0.6122f, 0.6626f, 0.7004f, 0.7541f, 0.7957f, 0.8279f, 
    0.8491f, 0.8732f, 0.8910f, 0.9092f, 0.9222f, 0.9319f, 0.9508f, 0.9650f
};

static float Get_Ascent_Time(float dist_m, float rate_m_min);

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

void Update_N2_Loadings_Schreiner(float current_loadings[], float depth_meters, float prev_depth_meters, float fraction_o2, float interval_sec) {
    
    float time_min = interval_sec / 60.0f;
    float fraction_n2 = 1.0f - fraction_o2;

    // 1. 시작 및 종료 시점의 폐포 내 질소 분압 계산
    float P_amb_start = 1.0f + (prev_depth_meters / 10.0f);
    float P_amb_end   = 1.0f + (depth_meters / 10.0f);
    
    // 수증기압 보정 (압력이 너무 낮아질 경우를 대비해 하한선 설정)
    float P_gas_n2_start = (P_amb_start > WATER_VAPOR_PRESSURE) ? (P_amb_start - WATER_VAPOR_PRESSURE) * fraction_n2 : 0.0f;
    float P_gas_n2_end   = (P_amb_end > WATER_VAPOR_PRESSURE) ? (P_amb_end - WATER_VAPOR_PRESSURE) * fraction_n2 : 0.0f;

    // 2. 분당 질소 압력 변화율 (R)
    float R = (P_gas_n2_end - P_gas_n2_start) / time_min;

    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
        float Pi = current_loadings[i]; 
        float half_life = N2_Half_Lives[i];
        float k = logf(2.0f) / half_life;
        
        float P_new;

        // 3. 수치적 안정성을 위한 분기 처리
        // 수심 변화가 거의 없거나(R=0), 시간이 0에 가까우면 할데인 공식 사용
        if (fabsf(R) < 0.00001f) {
            // 할데인(Haldane) 공식: P_new = Pi + (P_gas - Pi) * (1 - e^-kt)
            P_new = Pi + (P_gas_n2_end - Pi) * (1.0f - expf(-k * time_min));
        } else {
            // 표준 슈라이너(Schreiner) 공식
            float e_kt = expf(-k * time_min);
            // 공식: P_t = P_start + R*(t - 1/k) - (P_start - Pi - R/k) * e^-kt
            P_new = P_gas_n2_start + R * (time_min - 1.0f / k) - (P_gas_n2_start - Pi - (R / k)) * e_kt;
        }

        // 4. 안전 장치: 물리적으로 질소압이 0보다 작을 수 없음
        if (P_new < 0.0f) P_new = 0.0f;

        current_loadings[i] = P_new;
    }
}

float Calculate_NDL(float depth_meters, float current_loadings[], float GF_Hi, float fraction_o2) {
    float min_time = 999.0f;
    float P_surface = 1.01325f; // 표준 대기압
    float fraction_n2 = 1.0f - fraction_o2;
    
    // 핵심 수정: Update 함수와 동일하게 수증기압 보정 적용
    float P_amb = 1.0f + (depth_meters / 10.0f);
    float P_gas = (P_amb - WATER_VAPOR_PRESSURE) * fraction_n2;
    if (P_gas < 0) P_gas = 0;

    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
        // M-value 계산 (Buhlmann 공식)
        float M_surf = A_Coefficients[i] + (P_surface / B_Coefficients[i]);
        float P_tol = P_surface + GF_Hi * (M_surf - P_surface);

        if (current_loadings[i] > P_tol) return 0.0f; 
        if (P_gas <= P_tol) continue; 

        // NDL 역계산 공식
        float numerator = P_gas - P_tol;
        float denominator = P_gas - current_loadings[i];
        
        if (denominator <= 0.0001f) continue; 

        float time = (-N2_Half_Lives[i] / logf(2.0f)) * logf(numerator / denominator);

        if (time < min_time) min_time = time;
    }
    return (min_time > 999.0f) ? 999.0f : min_time;
}

/**
 * @brief 특정 수심이 안전한지 판단하는 함수 (GF 적용)
 * 
 * @param depth_to_check  검사할 수심 (m)
 * @param loadings        현재 조직 내 질소량
 * @param gf_value        적용할 GF 값 (첫 정지 수심 찾을 땐 GF_Low 사용)
 * @return true(안전함), false(위험함, 더 깊은 곳에서 정지해야 함)
 */
bool Is_Depth_Safe(float depth_to_check, float loadings[], float gf_low) {
    float P_amb = 1.0f + (depth_to_check / 10.0f); // 주변 압력

    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
        // 1. 순수 M-Value 계산
        float M_pure = A_Coefficients[i] + (P_amb / B_Coefficients[i]);
        
        // 2. GF가 적용된 허용 한계치 (M_GF) 계산
        // 공식: M_GF = P_amb + GF * (M_pure - P_amb)
        float M_gf = P_amb + gf_low * (M_pure - P_amb);
        // 3. 현재 조직의 질소압이 허용치를 넘는지 검사
        if (loadings[i] > M_gf) {
            return false; // 하나라도 넘으면 위험
        }
    }
    return true; // 모든 조직이 안전
}

/**
 * @brief 감압 정보를 계산하는 메인 함수
 * 
 * @param current_loadings 현재 조직 질소 배열
 * @param GF_Low           사용자 설정 GF Low (예: 0.30)
 * @param GF_High          사용자 설정 GF High (예: 0.70)
 * @return DecoPlan        정지 수심과 시간
 */
DecoPlan Calculate_Deco_Stop(float current_loadings[], float GF_Low, float GF_High, float PO2) {
    DecoPlan plan = {0, 0, false};
    
    // 임시 로딩 배열 (시뮬레이션 용)
    float sim_loadings[NUM_COMPARTMENTS];
    for(int i=0; i<NUM_COMPARTMENTS; i++) sim_loadings[i] = current_loadings[i];

    // 1. 수면(0m)이 안전한지 확인 (NDL 체크)
    // 수면에서는 GF High를 적용하여 검사
    if (Is_Depth_Safe(0.0, sim_loadings, GF_High)) {
        plan.is_deco = false; // 감압 불필요 (NDL 남음)
        return plan;
    }

    plan.is_deco = true;

    // 2. 감압 정지 수심(Ceiling) 찾기
    // 3m 부터 시작해서 3, 6, 9... 순으로 내려가며 안전한지 확인
    // 주의: 첫 정지 수심을 찾는 것이므로 GF_Low를 기준으로 함 (보수적 접근)
    int depth = 3;
    while (depth < 100) { // 최대 100m 제한 (무한루프 방지)
        if (Is_Depth_Safe((float)depth, sim_loadings, GF_Low)) {
            // 이 수심은 안전함! -> 여기가 바로 정지 수심
            plan.stop_depth = depth;
            break;
        }
        depth += 3; // 3m 더 깊이 내려가서 확인
    }

    // 3. 정지 시간 계산 (시뮬레이션)
    // 현재 발견한 stop_depth에서 얼마나 있어야, 다음 단계(stop_depth - 3m)로 갈 수 있나?
    
    int seconds = 0;
    float next_stop_depth = (float)(plan.stop_depth - 3);
    
    // GF 보간 (Interpolation): 
    // 현재 정지 수심에서는 GF_Low, 수면에서는 GF_High.
    // 하지만 다음 단계로 넘어가기 위한 기준은 그 사이의 어떤 GF 값임.
    // 편의상 여기서는 보수적으로 GF_Low를 유지하거나, 깊이에 따라 선형 보간해야 함.
    // *표준 구현*: 현재 정지 수심이 'Deepest Stop'이므로 GF_Low를 적용.
    // 시간이 지나서 다음 수심으로 갈 수 있는지 체크할 때도, 
    // 다음 수심에 해당하는 GF(선형 보간된 값)를 넘지 않는지 봐야 함.
    
    while (seconds < 3600) { // 최대 99분 제한 (99 * 60)
        // A. 1분 후의 질소 상태 예측 (기체는 Air 21% 가정, 혹은 현재 기체)
        // Update_N2_Loadings 함수 재사용 (시간 60초)
        Update_N2_Loadings(sim_loadings, (float)plan.stop_depth, PO2, 10.0);
        seconds += 10;

        // B. 이제 3m 위로 올라가도 안전한가?
        // 올라갈 목표 수심에 대해 적절한 GF 계산 (선형 보간)
        // Slope = (GF_High - GF_Low) / (0 - First_Stop_Depth)
        // GF_target = GF_High - (Slope * Target_Depth)
        // 하지만 First_Stop_Depth는 고정되어야 함 (최초 발견된 plan.stop_depth)
        
        float slope = (GF_High - GF_Low) / (0.0f - (float)plan.stop_depth); // 기울기는 음수 아님에 주의 (분모가 음수라 전체 양수화 필요하나 로직 점검 필요)
        // 정확한 로직: 깊을수록 GF 작음.
        // 분모: (0 - MaxDepth) -> 음수. 분자: (High - Low) -> 양수. 결과: 음수 기울기.
        // 식: GF = GF_Hi + Slope * Depth
        
        float gf_at_next_depth = GF_High + slope * next_stop_depth;

        // 목표 수심(next_stop_depth)이 안전한지 체크
        if (Is_Depth_Safe(next_stop_depth, sim_loadings, gf_at_next_depth)) {
            // 안전하다! 이제 올라가도 됨.
            plan.stop_time = (int)ceilf(seconds / 60.0f);
            break;
        }
    }

    return plan;
}

/**
 * @brief 현재 수심에서 특정 수심까지 상승하는 데 걸리는 시간 계산
 * @param dist_m 이동할 거리 (m)
 * @param rate_m_min 분당 상승 속도 (m/min)
 * @return float 소요 시간 (분)
 */
float Get_Ascent_Time(float dist_m, float rate_m_min) {
    if (dist_m <= 0) return 0.0f;
    return dist_m / rate_m_min;
}

/**
 * @brief TTS (Time To Surface) 계산 함수
 * 
 * @param current_depth_m 현재 수심
 * @param current_loadings 현재 조직 내 질소 로딩 배열 (변경되지 않음)
 * @param GF_Low          GF Low 값 (예: 0.30)
 * @param GF_High         GF High 값 (예: 0.70)
 * @param fraction_o2     사용 기체 산소 농도 (예: 0.21)
 * @return int            총 상승 시간 (분, 올림 처리)
 */
int Calculate_TTS(float current_depth_m, float current_loadings[], float GF_Low, float GF_High, float fraction_o2) {
    
    // 1. 시뮬레이션용 로딩 배열 복사 (원본 보존)
    float sim_loadings[NUM_COMPARTMENTS];
    for(int i=0; i<NUM_COMPARTMENTS; i++) {
        sim_loadings[i] = current_loadings[i];
    }

    float total_time_min = 0.0f;
    float sim_depth = current_depth_m;
    const float ASCENT_RATE = 18.0f; // 분당 18m 상승

    // 2. 무감압 한계(NDL) 이내인지 확인 (수면으로 바로 상승 가능한지 체크)
    // 수면 도착 시점의 안전도는 GF_High로 판단
    if (Is_Depth_Safe(0.0f, sim_loadings, GF_High)) {
        // 감압 없이 바로 상승 가능하므로 상승 시간만 더함
        total_time_min = Get_Ascent_Time(sim_depth, ASCENT_RATE);
        
        // 올림 처리하여 반환 (안전을 위해 분 단위 올림)
        return (int)ceilf(total_time_min);
    }

    // 3. 첫 번째 감압 정지 수심(Deepest Stop) 찾기
    // 바닥에서부터 3m 단위로 올라가며 GF_Low를 기준으로 안전한 첫 수심을 찾음
    int first_stop_depth = 0;
    for (int d = 3; d < (int)sim_depth; d += 3) {
        if (Is_Depth_Safe((float)d, sim_loadings, GF_Low)) {
            first_stop_depth = d;
            break;
        }
    }
    // 만약 계산상 현재 수심보다 더 깊은 곳이 첫 정지라면(이론상), 현재 수심을 첫 정지로 설정
    if (first_stop_depth == 0 || first_stop_depth > sim_depth) {
        first_stop_depth = (int)sim_depth; // 매우 위험한 상황이나 시뮬레이션 위해 설정
    }

    // 4. 현재 수심에서 첫 정지 수심까지 상승 시뮬레이션
    float ascent_dist = sim_depth - (float)first_stop_depth;
    float ascent_time = Get_Ascent_Time(ascent_dist, ASCENT_RATE);
    
    // 상승하는 동안에도 가스 교환(배출/흡수)이 일어남
    // 평균 수심에서 해당 시간만큼 머무른 것으로 근사 계산
    float avg_depth = (sim_depth + (float)first_stop_depth) / 2.0f;
    Update_N2_Loadings(sim_loadings, avg_depth, fraction_o2, ascent_time * 60.0f);
    
    total_time_min += ascent_time;
    sim_depth = (float)first_stop_depth;

    // 5. 감압 정지 및 단계별 상승 루프 (Deco Loop)
    // GF 기울기 계산 (Deepest Stop에서는 GF_Low, 수면에서는 GF_High)
    // 공식: GF_slope = (GF_High - GF_Low) / (0 - first_stop_depth)
    // 주의: 분모가 음수이므로 기울기는 음수가 됨 (깊이가 0에 가까워질수록 GF값 증가)
    float gf_slope = (GF_High - GF_Low) / (0.0f - (float)first_stop_depth);

    while (sim_depth > 0) {
        float next_depth = sim_depth - 3.0f;
        if (next_depth < 0) next_depth = 0.0f;

        // 다음 수심으로 가기 위해 필요한 목표 GF 계산 (선형 보간)
        float target_gf = GF_High + (gf_slope * next_depth);

        // 다음 수심(3m 위)이 안전한지 확인
        if (Is_Depth_Safe(next_depth, sim_loadings, target_gf)) {
            // 안전함 -> 다음 수심으로 이동 (3m 상승)
            float travel_time = Get_Ascent_Time(sim_depth - next_depth, ASCENT_RATE);
            
            // 이동 중 가스 교환 업데이트
            Update_N2_Loadings(sim_loadings, (sim_depth + next_depth)/2.0f, fraction_o2, travel_time * 60.0f);
            
            total_time_min += travel_time;
            sim_depth = next_depth;
        } else {
            // 안전하지 않음 -> 현재 수심에서 1분간 감압 정지
            Update_N2_Loadings(sim_loadings, sim_depth, fraction_o2, 60.0f); // 1분(60초) 경과
            total_time_min += 1.0f;
        }
        
        // 무한 루프 방지 (안전 장치)
        if (total_time_min > 999.0f) break;
    }

    // 최종 결과는 분 단위 정수로 올림 반환
    return (int)ceilf(total_time_min);
}