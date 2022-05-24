
#ifndef _DISPLAY_GHEALGORITHM_H_
#define _DISPLAY_GHEALGORITHM_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "DisplayPc.h"

#ifndef IN
#define IN
#endif

#ifndef FALSE
#define FALSE 0 
#endif

#ifndef TRUE
#define TRUE 1 
#endif


#define GlobalHist_MAX_BIN_INDEX (GlobalHist_BIN_COUNT - 1) // Index of last histogram bin
#define DD_MAX(a, b) ((a) < (b) ? (b) : (a))
#define DD_MIN(a, b) ((a) < (b) ? (a) : (b))
#define HUNDREDPERCENT_TO_VALUE(n) (n / 10000)    // Converts hundred percent to actual value
#define DD_ROUNDTONEARESTINT(p) (uint32_t)(p + 0.5)
#define DD_ABS(x) ((x) < 0 ? -(x) : (x))
#define DD_DIFF(a, b) (((a) > (b)) ? ((a) - (b)) : ((b) - (a)))
#define GlobalHist_DEFAULT_BOOST 1.0
#define GlobalHist_IET_SCALE_FACTOR 512           // IET factor programmed in 1.9 format, so fraction shift is (1 << 9)
#define MILLIUNIT_TO_UNIT(n) (n / 1000)    // Convert milli unit to unit
#define GlobalHist_IIR_FILTER_ORDER 3
#define GlobalHist_IET_MAX_VAL 1023                        // IET values are in 1.9 format (1 bit integer, 9 bit fraction)
#define GlobalHist_MAX_IET_INDEX (GlobalHist_IET_LUT_LENGTH - 1) // Index of last histogram bin
#define SMOOTHENING_MIN_STABLE_CUT_OFF_FREQUNCY_DURATION 500.0 // 500ms
#define PI 3.14159265358979323846               // Value of PI
#define PHASE_GlobalHist_PERIOD 50             // 50ms
#define GlobalHist_SMOOTHENING_MIN_SPEED_DEFAULT 500                 // Default GlobalHist temporal filter Min cutoff frequency in Milli Hz
#define GlobalHist_SMOOTHENING_MAX_SPEED_DEFAULT 800                 // Default GlobalHist temporal filter Min cutoff frequency in Milli Hz
#define GlobalHist_SMOOTHENING_PERIOD_DEFAULT 30                     // Default GlobalHist temporal timer period
#define GlobalHist_SMOOTHENING_TOLERANCE_DEFAULT 50                  // Default Minimum step percent to stop the timer. In 100 * percent
#define GlobalHist_IET_MAX_VAL    1023            // IET values are in 1.9 format (1 bit integer, 9 bit fraction)
#define SOLID_COLOR_POWER_THRESHOLD 0.95
#define SOLID_COLOR_SEARCH_WINDOW_SIZE 4


// GlobalHist Algorithm function pointer.
typedef bool (*PFN_GlobalHistALGORITHM)(IN void *,GlobalHist_ARGS *GheArgs);
typedef void (*PFN_GlobalHistRESETALGORITHM)(IN void *);
typedef void (*PFN_GlobalHistPROGRAMIETREGISTERS)(IN void *, GlobalHist_ARGS *GheArgs );

// Function table for GlobalHist functions.
typedef struct _GlobalHist_FUNCTBL
{
    PFN_GlobalHistALGORITHM pGheAlgorithm; 
  //  PFN_GlobalHistSETADJUSTMENTS pGheSetAdjustments;
    PFN_GlobalHistRESETALGORITHM pGheResetAlgorithm;
    PFN_GlobalHistPROGRAMIETREGISTERS pGheSetIet;
} GlobalHist_FUNCTBL;


typedef struct _GlobalHist_ALGORITHM
{
    uint32_t ImageSize;            // Source image size (pixels)
} GlobalHist_ALGORITHM;

typedef struct _GlobalHist_IE
{
     uint32_t LutApplied[GlobalHist_IET_LUT_LENGTH];
     uint32_t LutTarget[GlobalHist_IET_LUT_LENGTH]; 
     uint32_t LutDelta[GlobalHist_IET_LUT_LENGTH];
} GlobalHist_IE;

typedef struct _GlobalHist_TEMPORAL_FILTER_PARAMS
{
    uint64_t SmootheningIteration;
    uint32_t MinCutOffFreqInMilliHz; // Value from the INF or Default
    uint32_t MaxCutOffFreqInMilliHz; // Value from the INF or Default
    uint32_t CurrentMinCutOffFreqInMilliHz;
    uint32_t CurrentMaxCutOffFreqInMilliHz;
    uint32_t PrevHistogram[GlobalHist_BIN_COUNT];
    double IETHistory[GlobalHist_IET_LUT_LENGTH][GlobalHist_IIR_FILTER_ORDER];
    double TargetBoost;
    double MinimumStepPercent;
} GlobalHist_TEMPORAL_FILTER_PARAMS;


typedef struct _GlobalHist_CFG
{
    double MinimumStepPercent;
    double MinIIRCutOffFreq;
    double MaxIIRCutOffFreq;
    double MinPhaseInDuration;
    double MaxPhaseInDuration;

}GlobalHist_CFG;


typedef struct _GlobalHist_CONTEXT
{
    // Hardware dependent variables //
  
    GlobalHist_FUNCTBL GheFuncTable;      // GlobalHist Algorithm Function Table
    PIPE_ID Pipe;
    uint32_t Histogram[GlobalHist_BIN_COUNT]; // Bin wise histogram data for current frame.
    uint32_t LUT[GlobalHist_BIN_COUNT];
    GlobalHist_ALGORITHM Algorithm;
    // Runtime variables
    GlobalHist_IE ImageEnhancement;
     
    GlobalHist_TEMPORAL_FILTER_PARAMS FilterParams;
    GlobalHist_CFG GheCfg;

    double DeGammaLUT[GlobalHist_BIN_COUNT];

} GlobalHist_CONTEXT;


void DisplayInitializeAlgorithm(GlobalHist_CONTEXT *pGhe,GlobalHist_ARGS *GheArgs );
void DisplayInitializeTemporalIIRFilterParams(GlobalHist_CONTEXT *pGheContext);
double Apply1DLUT(double InVal, double *pLUT, double MaxIndex);

void DisplayGheAlgorithm(GlobalHist_CONTEXT *pGheContext,GlobalHist_ARGS *GheArgs);
bool TemporalSmoothenIET(GlobalHist_CONTEXT *pGheContext,GlobalHist_ARGS *GheArgs);
bool IsTargetIETReached(GlobalHist_CONTEXT *pGheContext);
void DisplaySetDietReg(GlobalHist_CONTEXT *pGheContext, GlobalHist_ARGS *GheArgs);
void DisplayResetAlgorithm(GlobalHist_CONTEXT *pGheContext);
double CalculateIIRFilterCoefficient(GlobalHist_CONTEXT *pGheContext);
double DisplayPcPhaseCoordinatorTemporalIIRFilterNthOrder(double FilterCoefficient, double Input, double *pInputHistoryPhase);
double GetSRGBDecodingValue(double input);
double EstimateProbabilityOfFullScreenSolidColor(double *pPowerHistogram, double TotalPower);
double GetRelativeFrameBrightnessChange(GlobalHist_CONTEXT* pGheContext,  uint32_t* pPrevHist);
#endif


