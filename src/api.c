#include <stdio.h>
#include "../include/api.h"
#include "../include/soil_data.h"
#include "../include/ml_integration.h"
#include "../include/fertilizer.h"
#include "../include/utils.h"

// You need to pass the Trie root and fertilizer hash map
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
    MLInput mlInput;
    mlInput.soil = soil;
    str_copy(mlInput.location, request->location, sizeof(mlInput.location));

    // Get ML prediction
    MLOutput mlOutput = get_crop_prediction(&mlInput);

    // Get fertilizer price
    float ureaPrice = get_fertilizer_price(fertMap, "Urea");
    if (ureaPrice < 0) {
        log_info("Urea fertilizer not found in hash map.");
    }

    // Print results
    printf("Location: %s\n", request->location);
    printf("Predicted Crop: %s\n", mlOutput.predictedCrop);
    if (ureaPrice >= 0)
        printf("Urea Price: %.2f\n", ureaPrice);
}
