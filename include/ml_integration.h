#ifndef ML_INTEGRATION_H
#define ML_INTEGRATION_H

typedef struct {
    char imagePath[256];
    float pH;
    int nitrogen;
    int phosphorus;
    int potassium;
    int moisture;
} MLInput;

typedef struct {
    char predictedSoil[64];
    char recommendedCrops[256];
} MLOutput;
int forward_image_to_ml(
    const char *image_path,
    char *response,
    size_t response_size
);
int forward_image_to_plant_ml(
    const char *image_path,
    char *response,
    size_t response_size
);

int get_crop_prediction(MLInput *in, MLOutput *out);

#endif
