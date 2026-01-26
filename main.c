#include <stdio.h>
#include <time.h>
#include <conio.h>
#include "buhlmann.h"

#define INTERVAL_SEC 1.0f  // 1초 간격

float current_loadings[16];

void showCurrentStatus(float loadings[], float NDL, float depth, int TTS) {
    printf("[%.1fm] ", depth);
    printf("[NDL: %.1f] ", NDL);
    printf("[TTS: %d] ", TTS);
    printf("[");
    for (int i = 0; i < NUM_COMPARTMENTS; i++) {
        printf("%.4f, ", loadings[i]);
    }
    printf("]\n");
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    time_t prevTime;
    int isOnDiving = 0;
    int ch;
    unsigned long count = 0;
    float current_depth = 0.0f;

    // 설정값이지만 일단 고정하고 진행..
    float GF_Hi = 0.8f;
    float GF_Lo = 0.2f;
    float PO2 = 0.21f;
    float NDL = 0.0f;
    int TTS = 0;

    DecoPlan deco = {0, 0, false};

    Init_Loadings(current_loadings);

    while(1) {
        printf("Press 's' to start the program or 'q' to quit: ");
        ch = getchar();
        if(ch == 's' || ch == 'S') {
            isOnDiving = 1;
            break;
        } else if(ch == 'q' || ch == 'Q') {
            printf("Program ended.\n");
            return 0;
        }
    }
    printf("###### DIVE Start!! ######\n");
    prevTime = time(NULL);
    while (isOnDiving) {
        time_t currentTime = time(NULL);
        float elapsed = (float)difftime(currentTime, prevTime);
        // INTERVAL_SEC 초마다 루프 실행
        if (INTERVAL_SEC <= elapsed) {
            count++;

            if(_kbhit()) {
                ch = _getch();
                if(ch == 'q' || ch == 'Q') {
                    isOnDiving = 0; // 'q' 또는 'Q' 입력 시 종료
                    continue;
                } else {
                    scanf_s("%f", &current_depth); // 깊이 입력 받기
                }
            }

            
            Update_N2_Loadings(current_loadings, current_depth, 0.21f, elapsed);
            NDL = Calculate_NDL(current_depth, current_loadings, GF_Hi, PO2);
            deco = Calculate_Deco_Stop(current_loadings, GF_Lo, GF_Hi, PO2);
            TTS = Calculate_TTS(current_depth, current_loadings, GF_Lo, GF_Hi, PO2);
            if(deco.is_deco) {
                printf(">>> Deco Stop Required at %dm for %d minutes <<<\n", deco.stop_depth, deco.stop_time);
            } else {
                showCurrentStatus(current_loadings, NDL, current_depth, TTS);                    
            }

            prevTime = currentTime;
        }
    }
    
    printf("Program ended.\n");
    return 0;
}