#include <stdio.h>
#include <time.h>
#include <conio.h>
#include "buhlmann.h"

/* 크로스 플랫폼 호환성을 위한 scanf 매크로 */
#ifdef _MSC_VER
#define SCANF_S(fmt, ...) scanf_s(fmt, __VA_ARGS__)
#else
#define SCANF_S(fmt, ...) scanf(fmt, __VA_ARGS__)
#endif

#define INTERVAL_SEC 1.0f // 1초 간격

float current_loadings[NUM_COMPARTMENTS]; // 질소 로딩

float loadings_n2[NUM_COMPARTMENTS]; // 질소 로딩
float loadings_he[NUM_COMPARTMENTS]; // 헬륨 로딩

void showCurrentStatus(float loadings[], float NDL, float depth, int TTS, DecoPlan deco, DiveStatus status, float PO2, int EAN)
{
    printf("[%.1fm] ", depth);
    printf("[EAN: %d] ", EAN);
    printf("[NDL: %.1f] ", NDL);
    printf("[Dive time: %ds] ", status.dive_time);
    printf("[PO2: %.2f] ", PO2);
    printf("[TTS: %d] ", TTS);
    printf("[Deco: %s, depth:%d, Time: %d] ", deco.is_deco ? "Yes" : "No", deco.stop_depth, deco.stop_time);
    printf("[Max Depth: %dm, Safe Stop Time: %ds]\n", status.max_depth, status.safe_stop_second);

    // show loadings
    // printf("[");
    // for (int i = 0; i < NUM_COMPARTMENTS; i++) {
    //     printf("%.4f, ", loadings[i]);
    // }
    // printf("]\r\n\r\n");
}

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    time_t prevTime;
    int isOnDiving = 0;
    int ch;
    unsigned long count = 0;
    float current_depth = 0.0f;
    float prev_depth = 0.0f;

    // 설정값이지만 일단 고정하고 진행..
    float GF_Hi = 0.8f;
    float GF_Lo = 0.2f;
    float PO2 = 0.21f;
    float PO2_Deep = 0.0f;
    float NDL = 0.0f;
    int TTS = 0;
    int EAN = 21; // Air 21% O2
    PO2 = EAN / 100.0f;
    PO2_Deep = PO2;

    DecoPlan deco = {0, 0, false};
    DiveStatus status = {0, 0};

    Init_Loadings(loadings_n2);

    // 원래는 현재 수심이 1.2m이상이면 바로 다이빙 시작이지만, 시뮬레이터에서는 's' 키를 눌러서 시작하도록 함
    while (1)
    {
        printf("Press 's' to start the program or 'q' to quit: ");
        fflush(stdout);
        ch = getchar();
        if (ch == 's' || ch == 'S')
        {
            isOnDiving = 1;
            break;
        }
        else if (ch == 'q' || ch == 'Q')
        {
            printf("Program ended.\n");
            fflush(stdout);
            return 0;
        }
    }
    printf("###### DIVE Start!! ######\n");
    fflush(stdout);
    prevTime = time(NULL);
    while (isOnDiving)
    {
        time_t currentTime = time(NULL);
        float elapsed = (float)difftime(currentTime, prevTime);
        // INTERVAL_SEC 초마다 루프 실행
        if (INTERVAL_SEC <= elapsed)
        {
            float process_step = 1.0f;

            status.dive_time += (int)elapsed;

            if (_kbhit())
            {
                ch = _getch();
                if (ch == 'd' || ch == 'D')
                { // 'd' 누르고 수심 입력
                    printf("\nEnter Depth: ");
                    scanf_s("%f", &current_depth);
                    if (current_depth > status.max_depth)
                    {
                        status.max_depth = (int)current_depth;
                    }
                }
                else if (ch == 'q' || ch == 'Q')
                {
                    isOnDiving = 0;
                    continue;
                }
                else if (ch == 'e' || ch == 'E')
                {
                    // EAN 조정
                    printf("\nEnter EAN: ");
                    scanf_s("%d", &EAN);
                    PO2 = EAN / 100.0f;
                }
            }

            PO2_Deep = PO2 * (1.0f + (current_depth / 10.0f)); // 수심에 따른 PO2 계산

            // 다이빙 종료 조건
            // 수심이 1.2m 이하로 내려가면 다이빙 종료 (수면 도착)
            if (current_depth <= 1.2f && 60 < status.dive_time)
            {
                isOnDiving = 0;
                continue;
            }

            // 10미터 초과 내려가면 안전정지 3분 생김
            if (10 < current_depth && status.safe_stop_second <= 0)
            {
                status.safe_stop_second = 180;
            }

            // 안전 정지 2분 추가 조건들 -->
            // 1. 깊은 수심 다이빙
            if (status.safe_stop_second == 180 && 30 < current_depth)
            {
                // 30미터 초과 내려가면 안전정지 2분 추가
                status.safe_stop_second += 120;
            }

            // 2. NDL에 근접한 다이빙
            if (status.safe_stop_second == 180 && NDL <= 5)
            {
                // NDL 5분 이하로 내려가면 안전정지 2분 추가
                status.safe_stop_second += 120;
            }

            // 3. 빠른 상승속도(분당 18미터 이상)로 상승한 경우
            float ascent_rate = (prev_depth - current_depth) / (elapsed / 60.0f); // 분당 상승 속도 계산
            if (status.safe_stop_second == 180 && ascent_rate > 18.0f)
            {
                // 빠른 상승으로 감지되면 안전정지 2분 추가
                status.safe_stop_second += 120;
            }

            // 4. 반복 다이빙 (하루 3회이상 다이빙을 하거나 수면휴식이 짧은 경우)

            // <-- 안전정지 2분 추가 조건들

            if (status.safe_stop_second > 0 && current_depth <= 6)
            {
                status.safe_stop_second--;
                if (status.safe_stop_second < 0)
                    status.safe_stop_second = 0;
            }

            // Update_N2_Loadings(current_loadings, current_depth, 0.21f, elapsed);
            Update_N2_Loadings_Schreiner(current_loadings, current_depth, prev_depth, PO2, process_step);
            NDL = Calculate_NDL(current_depth, current_loadings, GF_Hi, PO2);
            deco = Calculate_Deco_Stop(current_loadings, GF_Lo, GF_Hi, PO2);
            TTS = Calculate_TTS(current_depth, current_loadings, GF_Lo, GF_Hi, PO2);
            showCurrentStatus(current_loadings, NDL, current_depth, TTS, deco, status, PO2_Deep, EAN);

            prev_depth = current_depth;
            prevTime = currentTime;
        }
    }

    printf("###### DIVE End!! ######\n");
    return 0;
}