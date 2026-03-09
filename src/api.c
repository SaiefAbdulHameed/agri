#include <stdio.h>
#include <string.h>
#include "../include/api.h"
#include "../include/soil_data.h"

#include "../include/fertilizer.h"
#include "../include/utils.h"

void handle_request(SoilRequest* request, TrieNode* soilRoot, FertilizerNode* fertMap[]) {
    if (!request || !soilRoot || !fertMap) {
        log_error("Invalid parameters to handle_request.");
        return;
    }

    SoilData* soil = search_location(soilRoot, request->location);
    if (!soil) {
        log_info("No soil data found for this location.");
        return;
    }

    // Prepare ML input
    MLInput mlInput = {0};
    mlInput.pH = soil->pH;
    mlInput.nitrogen = soil->nitrogen;
    mlInput.phosphorus = soil->phosphorus;
    mlInput.potassium = soil->potassium;
    mlInput.moisture = soil->moisture;
    strcpy(mlInput.imagePath, ""); // optional

printf("[ML DEBUG] Sending to ML: pH=%.2f N=%d P=%d K=%d Moisture=%d\n",
       mlInput.pH,
       mlInput.nitrogen,
       mlInput.phosphorus,
       mlInput.potassium,
       mlInput.moisture);


    // Get ML prediction
    MLOutput mlOutput = {0};
    if (get_crop_prediction(&mlInput, &mlOutput) != 0) {
        log_error("ML prediction failed");
        strcpy(mlOutput.recommendedCrops, "Fallback: Wheat, Maize, Rice");
        strcpy(mlOutput.predictedSoil, "Unknown");
    }

    printf("[ML DEBUG] ML Response: Soil=%s, Crops=%s\n",
           mlOutput.predictedSoil, mlOutput.recommendedCrops);

    // Get fertilizer price
    float ureaPrice = get_fertilizer_price(fertMap, "Urea");
    if (ureaPrice < 0) {
        log_info("Urea fertilizer not found in hash map.");
    }

    // Print results
    printf("Location: %s\n", request->location);
    printf("Predicted Soil: %s\n", mlOutput.predictedSoil);
    printf("Recommended Crops: %s\n", mlOutput.recommendedCrops);
    if (ureaPrice >= 0)
        printf("Urea Price: %.2f\n", ureaPrice);
}
