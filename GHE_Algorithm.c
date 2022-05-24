#include "GHE_Algorithm.h"

void DisplayInitializeAlgorithm(GlobalHist_CONTEXT *pGheContext,GlobalHist_ARGS *GheArgs )
{

    pGheContext->GheFuncTable.pGheAlgorithm      = (PFN_GlobalHistALGORITHM)DisplayGheAlgorithm ;
    pGheContext->GheFuncTable.pGheSetIet         = (PFN_GlobalHistPROGRAMIETREGISTERS)DisplaySetDietReg;
    
    pGheContext->GheFuncTable.pGheResetAlgorithm = (PFN_GlobalHistRESETALGORITHM)DisplayResetAlgorithm;
      
    // Reset GlobalHist data structures
    pGheContext->GheFuncTable.pGheResetAlgorithm(pGheContext);

    DisplayInitializeTemporalIIRFilterParams(pGheContext);

    double HistLutStepSize = 1.0 / (double)GlobalHist_MAX_BIN_INDEX;

    for(uint8_t BinIndex=0; BinIndex<GlobalHist_MAX_BIN_INDEX; BinIndex++)
       pGheContext->DeGammaLUT[BinIndex] = GetSRGBDecodingValue((double)BinIndex * HistLutStepSize);
    
    // This call will manage the GlobalHist algorithm and activate Phase-In.
    pGheContext->GheFuncTable.pGheAlgorithm(pGheContext, GheArgs);
    
    // Program calculated DIET factor
    pGheContext->GheFuncTable.pGheSetIet(pGheContext, GheArgs);
      
}

double GetSRGBDecodingValue(double input)
{
   double output;

    if (input <= 0.04045)
    {
        output = input / 12.92;
    }
    else
    {
        output = pow(((input + 0.055) / 1.055), 2.4);
    }

    return output;
}

void DisplayInitializeTemporalIIRFilterParams(GlobalHist_CONTEXT *pGheContext)
{
    pGheContext->FilterParams.MinCutOffFreqInMilliHz = GlobalHist_SMOOTHENING_MIN_SPEED_DEFAULT;
    pGheContext->FilterParams.MaxCutOffFreqInMilliHz = GlobalHist_SMOOTHENING_MAX_SPEED_DEFAULT;
    pGheContext->FilterParams.CurrentMaxCutOffFreqInMilliHz = DD_MAX(pGheContext->FilterParams.MaxCutOffFreqInMilliHz, pGheContext->FilterParams.MinCutOffFreqInMilliHz);
    pGheContext->FilterParams.MinimumStepPercent = GlobalHist_SMOOTHENING_TOLERANCE_DEFAULT;
   
    for (uint8_t IetBinIndex = 0; IetBinIndex < GlobalHist_IET_LUT_LENGTH; IetBinIndex++)
    {
        for ( uint8_t FilterOrder = 0; FilterOrder < GlobalHist_IIR_FILTER_ORDER; FilterOrder++)
        {
            pGheContext->FilterParams.IETHistory[IetBinIndex][FilterOrder] = GlobalHist_IET_SCALE_FACTOR;
        }
    }

    for (uint8_t Count = 0; Count < GlobalHist_IET_LUT_LENGTH; Count++)
    {
        pGheContext->ImageEnhancement.LutTarget[Count] = GlobalHist_IET_SCALE_FACTOR;
    }

} 

void DisplayGheAlgorithm(GlobalHist_CONTEXT *pGheContext, GlobalHist_ARGS *GheArgs)
{ 
    uint32_t TotalNumOfPixel = 0;
    uint32_t *pMultiplierLut = pGheContext->ImageEnhancement.LutTarget;

    double EnhancementTable[GlobalHist_BIN_COUNT];
    double FilteredEnhancementTable[GlobalHist_BIN_COUNT];
    double BinIndexNormalized, IetVal;
    double CDFRange, CdfNormalizingFactor;
    double ProbabilityOfFullScreenSolidColor;
    double SumPower = 0; // Total Power of input frame
    double PowerDistribution[GlobalHist_BIN_COUNT]; // Histogram bin wise distribution of power

    const double MaxHistBinIndex = GlobalHist_MAX_BIN_INDEX;
    const double IetLutStepSize = 1.0 / (double)GlobalHist_MAX_IET_INDEX;
    const double HistBinStepSize = 1.0 / MaxHistBinIndex;
    const double MaxSlope = 7.0;
    const double MinSlope = 0.3;

    for (uint8_t BinIndex = 0; BinIndex < GlobalHist_BIN_COUNT; BinIndex++)
    {
        TotalNumOfPixel += pGheContext->Histogram[BinIndex];
        EnhancementTable[BinIndex] = TotalNumOfPixel;
    }

    pGheContext->Algorithm.ImageSize = TotalNumOfPixel;

    // Calculate histogram bin wise power distribution and total frame power
    for (uint8_t BinIndex = 0; BinIndex < GlobalHist_BIN_COUNT; BinIndex++)
    {
        double BinWeight = pGheContext->DeGammaLUT[BinIndex];
        PowerDistribution[BinIndex] = BinWeight * (double)pGheContext->Histogram[BinIndex];

        SumPower += PowerDistribution[BinIndex];
    }

    ProbabilityOfFullScreenSolidColor = EstimateProbabilityOfFullScreenSolidColor(PowerDistribution, SumPower);

    // Do not modify pixel values for Solid Color
    if (1 == ProbabilityOfFullScreenSolidColor)
    {
        for (uint8_t BinIndex = 0; BinIndex < GlobalHist_IET_LUT_LENGTH; BinIndex++)
        {
            pMultiplierLut[BinIndex] = GlobalHist_IET_SCALE_FACTOR;
        }

        return;
    }

    CDFRange = EnhancementTable[GlobalHist_BIN_COUNT - 1] - pGheContext->Histogram[0];
    CdfNormalizingFactor = 1.0 / CDFRange;
    EnhancementTable[0] = (double)(EnhancementTable[0] - pGheContext->Histogram[0]) * CdfNormalizingFactor;

    for (uint8_t BinIndex = 1; BinIndex < GlobalHist_BIN_COUNT; BinIndex++)
    {
        double OutVal = (EnhancementTable[BinIndex] - pGheContext->Histogram[0]) * CdfNormalizingFactor;
        double PrevSampleVal = EnhancementTable[BinIndex - 1];
        double Slope = MaxHistBinIndex * (OutVal - PrevSampleVal);

        Slope = DD_MAX(Slope, MinSlope);
        Slope = DD_MIN(Slope, MaxSlope);

        OutVal = PrevSampleVal + Slope * HistBinStepSize;
        OutVal = DD_MIN(OutVal, 1.0);

        EnhancementTable[BinIndex] = OutVal;
    }

    //No filtering for 0th and last values of the IET.
    FilteredEnhancementTable[0] = EnhancementTable[0];
    FilteredEnhancementTable[GlobalHist_MAX_BIN_INDEX] = EnhancementTable[GlobalHist_MAX_BIN_INDEX];

    //Smoothen jerks in FilteredEnhancementTable by averaging with nearby bins.
    for (uint8_t BinIndex = 1; BinIndex < GlobalHist_MAX_BIN_INDEX; BinIndex++)
    {
        //Average out current and two nearby sampels.
        FilteredEnhancementTable[BinIndex] = 0.333333 * (EnhancementTable[BinIndex - 1] + EnhancementTable[BinIndex] + EnhancementTable[BinIndex + 1]);
    }

    //Convert LUT of size GlobalHist_BIN_COUNT to IET LUT of size GlobalHist_IET_LUT_LENGTH
    for (uint32_t BinIndex = 1; BinIndex < GlobalHist_IET_LUT_LENGTH; BinIndex++)
    {
        BinIndexNormalized = (double)BinIndex * IetLutStepSize;
        IetVal = Apply1DLUT(BinIndexNormalized, FilteredEnhancementTable, MaxHistBinIndex);
        IetVal = IetVal / BinIndexNormalized;                   // Compute sample for multiplier LUT
        IetVal = DD_MIN(IetVal, 1.0 / BinIndexNormalized);      // Cap IET val to the value that will not cause clipping.

        IetVal = (double)GlobalHist_IET_SCALE_FACTOR * IetVal + 0.5;
        IetVal = DD_MIN(IetVal, GlobalHist_IET_MAX_VAL);
        IetVal = DD_MAX(IetVal, GlobalHist_IET_SCALE_FACTOR); // Never dim any pixel

        pMultiplierLut[BinIndex] = IetVal;
    }

    pMultiplierLut[0] = pMultiplierLut[1]; // 0th multiplier sample can't be computed. Extend 1st sample to the 0th 
    
    TemporalSmoothenIET(pGheContext,GheArgs );

    memcpy(pGheContext->FilterParams.PrevHistogram, pGheContext->Histogram, sizeof(pGheContext->Histogram));

}

double EstimateProbabilityOfFullScreenSolidColor(double *pPowerHistogram, double TotalPower)
{
    const double SolidColorPowerThreshold = SOLID_COLOR_POWER_THRESHOLD * TotalPower;
    double WindowSizeToProbabilityMapping[SOLID_COLOR_SEARCH_WINDOW_SIZE] = { 1, 1, 0.75, 0.375 };
    uint8_t N;

    // Find N number of consecutive bins which contain SOLID_COLOR_POWER_THRESHOLD amount of frame power.
    // N = 1 means solid color for sure. Since SOLID_COLOR_POWER_THRESHOLD is not 1.0, it detects almost solid color.
    // N = 2 may mean near solid color. One pixel value shift will shift energy to next or prev bin.
    // For example, image with solid color patches 247 and 248 will look almost single solid, but teh histogram will be spread across two bins.
    // N >= 3 means probability of solid color is less. Return value is gradullay reduced to 0 for N >= 3.
    for (N = 1; N <= SOLID_COLOR_SEARCH_WINDOW_SIZE; N++)
    {
        for (uint8_t BinIndex = 0; BinIndex < (GlobalHist_BIN_COUNT - N + 1); BinIndex++)
        {
            double SumPower = 0;
            for (uint8_t i = BinIndex; i < (BinIndex + N); i++)
            {
                SumPower += pPowerHistogram[i];

                if (SumPower >= SolidColorPowerThreshold)
                {
                    return WindowSizeToProbabilityMapping[N - 1];
                }
            }
        }
    }

    return 0;
}

 // Interpolate IET LUT from histogram based LUT.
 // This is required because IET has more entries than histogram LUT
 // Interpolation Logic => MinValue + (MaxValue - MinValue) * Interpolator.
double Apply1DLUT(double InVal, double *pLUT, double MaxIndex)
{
    uint32_t Index1, Index2;
    double Val1, Val2, Interpolator;
    double DIndex = InVal * MaxIndex;

    Index1 = (uint32_t)DIndex;
    Index2 = (uint32_t)(ceil(DIndex));

    Interpolator = (DIndex - (double)Index1);
    Val1         = pLUT[Index1];
    Val2         = pLUT[Index2];

    return Val1 + Interpolator * (Val2 - Val1);
}

void DisplaySetDietReg(GlobalHist_CONTEXT *pGheContext,GlobalHist_ARGS *GheArgs )
{ 

    GheArgs->PipeId        = pGheContext->Pipe;
    GheArgs->IsProgramDiet = TRUE;
   
    for ( uint8_t Count = 0; Count < GlobalHist_IET_LUT_LENGTH; Count++)
    {
        GheArgs->DietFactor[Count] = pGheContext->ImageEnhancement.LutApplied[Count];
    } 
}


void DisplayResetAlgorithm(GlobalHist_CONTEXT *pGheContext)
{

    for (uint8_t Count = 0; Count < GlobalHist_IET_LUT_LENGTH; Count++)
    {
        // Initialize to Slope of 1
        pGheContext->ImageEnhancement.LutApplied[Count] = GlobalHist_IET_SCALE_FACTOR;
  

        for (uint8_t FilterOrder = 0; FilterOrder < GlobalHist_IIR_FILTER_ORDER; FilterOrder++)
        {
            pGheContext->FilterParams.IETHistory[Count][FilterOrder] = GlobalHist_IET_SCALE_FACTOR;
        }
    }

    // Reset filter params
    pGheContext->FilterParams.TargetBoost         = GlobalHist_DEFAULT_BOOST;
    pGheContext->FilterParams.SmootheningIteration = 0;
}


bool TemporalSmoothenIET(GlobalHist_CONTEXT *pGheContext, GlobalHist_ARGS *GheArgs)
{
 
    double TemporalFilterCoefficient;
    double MaxAcceptableDelta;

    MaxAcceptableDelta = pGheContext->FilterParams.MinimumStepPercent;
    
    if (FALSE == IsTargetIETReached(pGheContext))
    { 

        TemporalFilterCoefficient = CalculateIIRFilterCoefficient(pGheContext);
        
	for (uint8_t Count = 0; Count < GlobalHist_IET_LUT_LENGTH; Count++)
	{
            double IetVal= (uint32_t)DisplayPcPhaseCoordinatorTemporalIIRFilterNthOrder( TemporalFilterCoefficient, 
			           pGheContext->ImageEnhancement.LutTarget[Count],&pGheContext->FilterParams.IETHistory[Count][0]) + 0.5 ;
            IetVal = DD_MIN(IetVal, GlobalHist_IET_MAX_VAL);
	    pGheContext->ImageEnhancement.LutApplied[Count] = IetVal;
	
	}

    }
    else
    {
        memcpy(pGheContext->ImageEnhancement.LutApplied ,pGheContext->ImageEnhancement.LutTarget , sizeof(pGheContext->ImageEnhancement.LutTarget));    
         
       for (uint8_t Count = 0; Count < GlobalHist_IET_LUT_LENGTH; Count++)
        {
            for (uint8_t HistoryIndex = 0; HistoryIndex < GlobalHist_IIR_FILTER_ORDER; HistoryIndex++)
            {
                pGheContext->FilterParams.IETHistory[Count][HistoryIndex] = pGheContext->ImageEnhancement.LutTarget[Count];
            }
        }
    
    }

    memcpy(pGheContext->FilterParams.PrevHistogram, pGheContext->Histogram, sizeof(pGheContext->FilterParams.PrevHistogram)); 

}

bool IsTargetIETReached(GlobalHist_CONTEXT *pGheContext)
{
    double MaxAcceptableDelta;
    double IETDelta;
    double LinearBoostDelta;

    MaxAcceptableDelta = pGheContext->FilterParams.MinimumStepPercent / 100.0 ;
    for (uint32_t i = 0; i < GlobalHist_IET_LUT_LENGTH; i++)
    {
        IETDelta = abs((double)pGheContext->ImageEnhancement.LutApplied[i] - (double)pGheContext->ImageEnhancement.LutTarget[i]) / (double)pGheContext->ImageEnhancement.LutTarget[i];

        if (IETDelta > MaxAcceptableDelta)
        {
             return FALSE;
         }
    }
        return TRUE;

}

double CalculateIIRFilterCoefficient(GlobalHist_CONTEXT *pGheContext)
{

    double AngularFrequency, TemporalFilterCoefficient;
    double MinCutoffFreq, MaxCutoffFreq;
    double CutOffFreq=0;
    double MinGheSmootheningPeriod;
    GlobalHist_TEMPORAL_FILTER_PARAMS *pFilterParams;

    pFilterParams            = &pGheContext->FilterParams;
    MinCutoffFreq            = MILLIUNIT_TO_UNIT((double)pFilterParams->CurrentMinCutOffFreqInMilliHz);
    MaxCutoffFreq            = MILLIUNIT_TO_UNIT((double)pFilterParams->CurrentMaxCutOffFreqInMilliHz);
    MinGheSmootheningPeriod = (double)(332225.9136) / (double)(10 * 1000 * 1000); //SampingFreq.
  
    double RelativeFrameBrightnessChange = GetRelativeFrameBrightnessChange(pGheContext, pFilterParams->PrevHistogram);
    CutOffFreq = MinCutoffFreq + (MaxCutoffFreq - MinCutoffFreq) * RelativeFrameBrightnessChange;

    AngularFrequency = 6.2831853 * CutOffFreq;
    TemporalFilterCoefficient = (MinGheSmootheningPeriod * AngularFrequency) / (1 + (MinGheSmootheningPeriod * AngularFrequency));

    return TemporalFilterCoefficient;
}


double DisplayPcPhaseCoordinatorTemporalIIRFilterNthOrder(double FilterCoefficient, double Input, double *pInputHistoryPhase)
{
    double AdjustedValue = Input;

    for (uint8_t Count = 0; Count < GlobalHist_IIR_FILTER_ORDER; Count++)
    {
        AdjustedValue = FilterCoefficient * AdjustedValue;
        AdjustedValue += (1 - FilterCoefficient) * pInputHistoryPhase[Count];
        pInputHistoryPhase[Count] = AdjustedValue;
    }

    return AdjustedValue;
}

double GetRelativeFrameBrightnessChange(GlobalHist_CONTEXT* pGheContext,  uint32_t* pPrevHist)
{
    double FrameAveragePowerPrev = 0;
    double FrameAveragePowerCurr = 0;
    double HistChangePercentage = 0;
    
    for ( uint32_t Count = 0; Count < GlobalHist_BIN_COUNT; Count++)
    {
        FrameAveragePowerPrev += pGheContext->DeGammaLUT[Count] * (double)pPrevHist[Count];
        FrameAveragePowerCurr += pGheContext->DeGammaLUT[Count] * (double)pGheContext->Histogram[Count];
    }

    if (FrameAveragePowerPrev > 0)
    {
        HistChangePercentage = DD_ABS(FrameAveragePowerCurr - FrameAveragePowerPrev) / FrameAveragePowerPrev;
    }
    else if (FrameAveragePowerPrev != FrameAveragePowerCurr)
    {
        HistChangePercentage = 1.0;
    }

    return DD_MIN(HistChangePercentage, 1.0);
}







