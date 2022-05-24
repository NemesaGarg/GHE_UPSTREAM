#include "GHE_Algorithm.h"


void SetHistogramDataBin(GlobalHist_ARGS *GheArgs)
{
  
   GlobalHist_CONTEXT* pGheContext = (GlobalHist_CONTEXT *)malloc(sizeof(GlobalHist_CONTEXT));
   
   memcpy(pGheContext->Histogram, GheArgs->Histogram, GlobalHist_BIN_COUNT * sizeof(uint32_t));    
   
   pGheContext->Algorithm.ImageSize = (GheArgs->Resolution_X * GheArgs->Resolution_Y);

   DisplayInitializeAlgorithm(pGheContext, GheArgs);

}




