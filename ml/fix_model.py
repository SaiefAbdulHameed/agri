import pandas as pd
from sklearn.ensemble import RandomForestClassifier
import joblib

# Simple training data - 7 features, 4 common crops
data = {
    'nitrogen':     [90, 85, 40, 100, 80, 45, 95, 35],
    'phosphorus':   [42, 40, 20, 60, 50, 25, 45, 18],
    'potassium':    [43, 45, 30, 70, 55, 35, 48, 28],
    'ph':           [6.5, 6.2, 7.5, 6.8, 7.0, 7.8, 6.0, 5.5],
    'moisture':     [60, 65, 40, 80, 50, 35, 70, 30],
    'temperature':  [25, 26, 30, 28, 22, 32, 24, 20],
    'rainfall':     [200, 220, 100, 250, 180, 90, 210, 80],
    'crop':         ['rice', 'rice', 'cotton', 'sugarcane', 
                     'wheat', 'cotton', 'rice', 'maize']
}

df = pd.DataFrame(data)

# Train model
X = df[['nitrogen', 'phosphorus', 'potassium', 'ph', 'moisture', 'temperature', 'rainfall']]
y = df['crop']

model = RandomForestClassifier(n_estimators=50, random_state=42)
model.fit(X, y)

# Save
joblib.dump(model, 'crop_recommendation_model.pkl')
print(f"✅ Fixed! Model now knows: {list(model.classes_)}")
