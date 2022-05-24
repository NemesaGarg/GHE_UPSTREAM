/**
 *
 * @file  DisplayPc.h
 * @brief  This file contains all the Display GHE  related interface functions
 *
 */

#ifndef _DISPLAY_PC_H_
#define _DISPLAY_PC_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef IN
#define IN
#endif

#ifndef FALSE
#define FALSE 0 
#endif

#ifndef TRUE
#define TRUE 1 
#endif



#define GlobalHist_BIN_COUNT 32   // Total number of segments in GlobalHist
#define GlobalHist_IET_LUT_LENGTH 33 // Total number of IET entries

typedef enum _PIPE_ID  {
    NULL_PIPE = 0x7F,
    GlobalHist_PIPE_ANY = 0x7E,
    GlobalHist_PIPE_A = 0,
    GlobalHist_PIPE_B = 1,
    GlobalHist_PIPE_C = 2,
    GlobalHist_PIPE_D = 3
} PIPE_ID;

typedef struct _DD_GlobalHist_ARGS
{
    PIPE_ID PipeId;
    bool IsProgramDiet;
    uint32_t DietFactor[GlobalHist_IET_LUT_LENGTH];
    uint32_t Histogram[GlobalHist_BIN_COUNT];
    uint32_t Resolution_X;
    uint32_t Resolution_Y;
}GlobalHist_ARGS;


void SetHistogramDataBin(GlobalHist_ARGS *GheArgs);

                 


#endif





