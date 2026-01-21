#include "../include/ml_integration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MLOutput get_crop_prediction(MLInput* input) {
    MLOutput out;
    strcpy(out.predictedCrop, "Unknown");

    // Write soil data to a temp file
    char tmpFile[] = "/tmp/soil_XXXXXX.json";
    int fd = mkstemp(tmpFile);
    if (fd < 0) return out;

    FILE* f = fdopen(fd, "w");
    if (!f) return out;

    fprintf(f, "{\"pH\":%.2f,\"moisture\":%.2f,\"nitrogen\":%.2f}\n",
            input->soil->pH,
            input->soil->moisture,
            input->soil->nitrogen);
    fclose(f);

    // Run python with temp file
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "python3 ml/ml_model.py %s", tmpFile);

    FILE* rp = popen(cmd, "r");
    if (!rp) {
        remove(tmpFile);
        return out;
    }

    char buffer[128];
    if (fgets(buffer, sizeof(buffer), rp)) {
        sscanf(buffer, "{\"crop\":\"%49[^\"]\"}", out.predictedCrop);
    }

    pclose(rp);
    remove(tmpFile);
    return out;
}
