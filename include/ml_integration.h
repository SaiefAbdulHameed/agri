#ifndef ML_INTEGRATION_H
#define ML_INTEGRATION_H

#include "soil_data.h"

// Structs for ML input/output
typedef struct MLInput {
    SoilData* soil;
    char location[100];
} MLInput;

typedef struct MLOutput {
    char predictedCrop[50];
} MLOutput;

// Placeholder functions
MLOutput get_crop_prediction(MLInput* input);

#endif
