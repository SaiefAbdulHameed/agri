#!/usr/bin/env python3
import sys
import json

# Expect one argument: path to JSON file
if len(sys.argv) != 2:
    print(json.dumps({"crop": "Unknown"}))
    sys.exit(0)

filename = sys.argv[1]

try:
    with open(filename, "r") as f:
        data = json.load(f)
except:
    print(json.dumps({"crop": "Unknown"}))
    sys.exit(0)

ph = data.get("pH", 0)
moisture = data.get("moisture", 0)
n = data.get("nitrogen", 0)

# Simple rules
if ph >= 6.0 and moisture > 60:
    crop = "Rice"
elif ph >= 5.5 and n > 30:
    crop = "Wheat"
elif ph >= 5.0:
    crop = "Millet"
else:
    crop = "Unknown"

# Return JSON
print(json.dumps({"crop": crop}))
