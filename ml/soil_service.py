#!/usr/bin/env python3
import tensorflow as tf
import numpy as np
from flask import Flask, request, jsonify
from flask_cors import CORS
from PIL import Image
import joblib
import sqlite3
import json
import os
from threading import Lock, Thread
import time

# =========================
# CONFIG
# =========================
SOIL_MODEL_PATH = "Soil_Model_VGG16_FineTuned.h5"
SOIL_LABELS_PATH = "updated_labels.txt"
PLANT_MODEL_PATH = "Disease_Model_MobileNetV2_Improved_2.h5"
PLANT_LABELS_PATH = "disease_labels.txt"
CROP_MODEL_PATH = "crop_recommendation_model.pkl"
DB_PATH = "../data/users.db"

IMG_SIZE = (224, 224)
model_lock = Lock()

# =========================
# APP INIT
# =========================
app = Flask(__name__)
CORS(app)

# =========================
# LOAD MODELS
# =========================
print("⏳ Loading models...")

try:
    soil_model = tf.keras.models.load_model(SOIL_MODEL_PATH)
    soil_labels = open(SOIL_LABELS_PATH).read().splitlines()
    print(f"✅ Soil model: {len(soil_labels)} classes")
except Exception as e:
    print(f"❌ Soil model failed: {e}")
    soil_model = None
    soil_labels = []

try:
    plant_model = tf.keras.models.load_model(PLANT_MODEL_PATH)
    plant_labels = open(PLANT_LABELS_PATH).read().splitlines()
    print(f"✅ Plant model: {len(plant_labels)} classes")
except Exception as e:
    print(f"❌ Plant model failed: {e}")
    plant_model = None
    plant_labels = []

try:
    crop_model = joblib.load(CROP_MODEL_PATH)
    crop_classes = crop_model.classes_
    print(f"✅ Crop model: {len(crop_classes)} classes")
except Exception as e:
    print(f"❌ Crop model failed: {e}")
    crop_model = None
    crop_classes = []

print("🚀 Models loaded")

# =========================
# DATABASE HELPERS
# =========================
def get_db_connection():
    return sqlite3.connect(DB_PATH)

def get_unprocessed_training_data(data_type, limit=100):
    """Get verified but unprocessed training data"""
    conn = get_db_connection()
    cursor = conn.cursor()
    
    cursor.execute("""
        SELECT id, input_data, output_data 
        FROM ml_training_data 
        WHERE data_type=? AND verified=1 AND processed=0
        LIMIT ?
    """, (data_type, limit))
    
    rows = cursor.fetchall()
    conn.close()
    return rows

def mark_data_processed(data_ids, accuracy=0.0):
    """Mark training data as processed"""
    if not data_ids:
        return
    
    conn = get_db_connection()
    cursor = conn.cursor()
    
    cursor.executemany("""
        UPDATE ml_training_data 
        SET processed=1, processed_at=CURRENT_TIMESTAMP, accuracy_improvement=?
        WHERE id=?
    """, [(accuracy, id) for id in data_ids])
    
    conn.commit()
    conn.close()
    print(f"✅ Marked {len(data_ids)} samples as processed")

def get_training_stats():
    """Get ML training statistics"""
    conn = get_db_connection()
    cursor = conn.cursor()
    
    stats = {}
    for dtype in ['crop', 'soil', 'disease']:
        cursor.execute("""
            SELECT COUNT(*) FROM ml_training_data WHERE data_type=? AND processed=0
        """, (dtype,))
        stats[dtype] = cursor.fetchone()[0]
    
    cursor.execute("SELECT COUNT(*) FROM ml_training_data WHERE processed=1")
    stats['total_processed'] = cursor.fetchone()[0]
    
    conn.close()
    return stats

# =========================
# INCREMENTAL LEARNING
# =========================
class IncrementalCropModel:
    """
    Online learning for crop recommendation.
    Uses warm_start for incremental RandomForest updates.
    """
    
    def __init__(self, base_model):
        self.model = base_model
        self.is_fitted = True
        self.buffer = []
        self.buffer_size_threshold = 10  # Retrain every N samples
        self.total_samples = 0
        
    def add_training_sample(self, features, crop):
        """Add single sample to buffer"""
        self.buffer.append((features, crop))
        self.total_samples += 1
        
        print(f"📊 Buffer: {len(self.buffer)}/{self.buffer_size_threshold}")
        
        if len(self.buffer) >= self.buffer_size_threshold:
            return self._retrain()
        return {"status": "buffered", "buffer_size": len(self.buffer)}
    
    def _retrain(self):
        """Incremental retraining with buffered samples"""
        if not self.buffer:
            return {"status": "no_data"}
        
        try:
            # Prepare data
            X_new = np.array([f for f, _ in self.buffer])
            y_new = np.array([c for _, c in self.buffer])
            
            print(f"🔄 Retraining with {len(self.buffer)} new samples...")
            
            with model_lock:
                # Get current accuracy before retraining
                # (In production, use validation set)
                
                # For RandomForest, we need to refit with combined data
                # Since we don't store all historical data, we use warm_start trick
                if hasattr(self.model, 'warm_start'):
                    # Increase n_estimators and fit on new data only
                    # This appends new trees (approximates incremental learning)
                    old_n = self.model.n_estimators
                    self.model.n_estimators += 10
                    self.model.warm_start = True
                    
                    # Partial fit on new data
                    self.model.fit(X_new, y_new)
                    
                    print(f"   Trees: {old_n} → {self.model.n_estimators}")
                else:
                    # Fallback: just fit on new data (loses old patterns!)
                    self.model.fit(X_new, y_new)
                    print("   Warning: Full refit (no warm_start)")
            
            # Save updated model
            backup_path = CROP_MODEL_PATH + ".backup"
            joblib.dump(self.model, backup_path)
            os.replace(backup_path, CROP_MODEL_PATH)
            
            processed_count = len(self.buffer)
            self.buffer = []
            
            return {
                "status": "retrained",
                "samples_processed": processed_count,
                "total_samples": self.total_samples,
                "n_estimators": self.model.n_estimators
            }
            
        except Exception as e:
            print(f"❌ Retrain failed: {e}")
            return {"status": "error", "error": str(e)}

# Initialize incremental learner
crop_learner = IncrementalCropModel(crop_model) if crop_model else None

# =========================
# AUTO-RETRAINING THREAD
# =========================
def auto_retrain_loop():
    """Background thread for periodic retraining"""
    while True:
        time.sleep(60)  # Check every minute
        
        if crop_learner is None:
            continue
        
        # Check for pending data
        pending = get_unprocessed_training_data('crop', limit=50)
        
        if len(pending) >= 10:
            print(f"🤖 Auto-retrain triggered: {len(pending)} pending samples")
            
            processed_ids = []
            for row_id, input_json, output_json in pending:
                try:
                    inp = json.loads(input_json)
                    out = json.loads(output_json)
                    
                    features = [
                        inp.get('nitrogen', 0),
                        inp.get('phosphorus', 0),
                        inp.get('potassium', 0),
                        inp.get('ph', 0),
                        inp.get('moisture', 0),
                        inp.get('temperature', 0),
                        inp.get('rainfall', 0)
                    ]
                    crop = out.get('result', 'rice')
                    
                    result = crop_learner.add_training_sample(features, crop)
                    
                    if result.get('status') == 'retrained':
                        processed_ids.append(row_id)
                        
                except Exception as e:
                    print(f"   Error processing row {row_id}: {e}")
                    processed_ids.append(row_id)  # Mark as processed even if failed
            
            # Mark all as processed
            mark_data_processed(processed_ids, accuracy=0.0)

# Start background thread
retrain_thread = Thread(target=auto_retrain_loop, daemon=True)
retrain_thread.start()

# =========================
# API ENDPOINTS
# =========================

@app.route("/health")
def health():
    return jsonify({
        "status": "ok",
        "models": {
            "soil": soil_model is not None,
            "plant": plant_model is not None,
            "crop": crop_model is not None
        },
        "training_stats": get_training_stats()
    })

@app.route("/predict-soil", methods=["POST"])
def predict_soil():
    if soil_model is None:
        return jsonify({"success": False, "error": "Soil model not loaded"}), 503
    
    if "image" not in request.files:
        return jsonify({"success": False, "error": "No image provided"}), 400
    
    try:
        img = preprocess_image(request.files["image"])
        preds = soil_model.predict(img)
        idx = int(np.argmax(preds))
        
        # FIXED: Proper ternary with else
        soil_type = soil_labels[idx] if idx < len(soil_labels) else f"Class_{idx}"
        
        return jsonify({
            "success": True,
            "soilType": soil_type,
            "confidence": float(preds[0][idx] * 100)
        })
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route("/predict-plant", methods=["POST"])
def predict_plant():
    if plant_model is None:
        return jsonify({"success": False, "error": "Plant model not loaded"}), 503
    
    if "image" not in request.files:
        return jsonify({"success": False, "error": "No image provided"}), 400
    
    try:
        img = preprocess_image(request.files["image"])
        preds = plant_model.predict(img)
        idx = int(np.argmax(preds))
        
        # FIXED: Proper ternary with else
        disease = plant_labels[idx] if idx < len(plant_labels) else f"Class_{idx}"
        
        return jsonify({
            "success": True,
            "disease": disease,
            "confidence": float(preds[0][idx] * 100)
        })
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route("/predict-soil-params", methods=["POST"])
def predict_soil_params():
    if crop_model is None:
        return jsonify({"success": False, "error": "Crop model not loaded"}), 503
    
    try:
        data = request.get_json()
        print(f"DATA RECEIVED: {data}")
        
        values = [
            float(data.get("nitrogen", 0)),
            float(data.get("phosphorus", 0)),
            float(data.get("potassium", 0)),
            float(data.get("ph", 0)),
            float(data.get("moisture", 0)),
            float(data.get("temperature", 0)),
            float(data.get("rainfall", 0))
        ]
        print(f"VALUES: {values}")
        
        with model_lock:
            model = crop_learner.model if crop_learner else crop_model
            X = np.array([values])
            print(f"INPUT SHAPE: {X.shape}")
            
            probs = model.predict_proba(X)[0]
            classes = model.classes_
        
        ranked = sorted(zip(classes, probs), key=lambda x: x[1], reverse=True)[:3]
        
        return jsonify({
            "success": True,
            "recommendations": [
                {"crop": crop, "confidence": round(prob * 100, 2)}
                for crop, prob in ranked
            ]
        })
        
    except Exception as e:
        import traceback
        print(f"ERROR: {str(e)}")
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500

# =========================
# ADMIN ML ENDPOINTS
# =========================

@app.route("/admin/ml/learn", methods=["POST"])
def admin_ml_learn():
    """
    Real-time learning endpoint for admin panel.
    Immediately adds training sample and triggers retrain if threshold reached.
    """
    if crop_learner is None:
        return jsonify({"success": False, "error": "Crop learner not initialized"}), 503
    
    try:
        data = request.get_json()
        
        features = [
            data.get('n', data.get('nitrogen', 0)),
            data.get('p', data.get('phosphorus', 0)),
            data.get('k', data.get('potassium', 0)),
            data.get('ph', 0),
            data.get('moisture', 0),
            data.get('temp', data.get('temperature', 0)),
            data.get('rain', data.get('rainfall', 0))
        ]
        
        crop = data.get('expected_crop', data.get('crop', 'rice'))
        
        # Add to learner
        result = crop_learner.add_training_sample(features, crop)
        
        # If retrained, mark DB entries as processed
        if result.get('status') == 'retrained':
            # Mark recent entries as processed
            pending = get_unprocessed_training_data('crop', limit=result['samples_processed'])
            mark_data_processed([r[0] for r in pending], accuracy=0.0)
        
        return jsonify({
            "success": True,
            "result": result,
            "total_samples": crop_learner.total_samples,
            "training_stats": get_training_stats()
        })
        
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route("/admin/ml/status")
def admin_ml_status():
    """Get detailed ML training status"""
    return jsonify({
        "success": True,
        "training_stats": get_training_stats(),
        "learner": {
            "initialized": crop_learner is not None,
            "total_samples": crop_learner.total_samples if crop_learner else 0,
            "buffer_size": len(crop_learner.buffer) if crop_learner else 0,
            "threshold": crop_learner.buffer_size_threshold if crop_learner else 0
        },
        "models": {
            "soil": {"loaded": soil_model is not None, "classes": len(soil_labels)},
            "plant": {"loaded": plant_model is not None, "classes": len(plant_labels)},
            "crop": {"loaded": crop_model is not None, "classes": len(crop_classes), "live": crop_learner is not None}
        }
    })

@app.route("/admin/ml/trigger-retrain", methods=["POST"])
def trigger_retrain():
    """Manually trigger retraining with all pending data"""
    if crop_learner is None:
        return jsonify({"success": False, "error": "Learner not ready"}), 503
    
    pending = get_unprocessed_training_data('crop', limit=100)
    
    if not pending:
        return jsonify({"success": True, "message": "No pending data to train", "trained": False})
    
    processed_ids = []
    results = []
    
    for row_id, input_json, output_json in pending:
        try:
            inp = json.loads(input_json)
            out = json.loads(output_json)
            
            features = [
                inp.get('nitrogen', 0),
                inp.get('phosphorus', 0),
                inp.get('potassium', 0),
                inp.get('ph', 0),
                inp.get('moisture', 0),
                inp.get('temperature', 0),
                inp.get('rainfall', 0)
            ]
            crop = out.get('result', 'rice')
            
            result = crop_learner.add_training_sample(features, crop)
            results.append(result)
            processed_ids.append(row_id)
            
        except Exception as e:
            print(f"Error on row {row_id}: {e}")
            processed_ids.append(row_id)  # Don't retry failed rows
    
    # Mark processed
    mark_data_processed(processed_ids)
    
    # Check if any actual retraining happened
    retrained = any(r.get('status') == 'retrained' for r in results)
    
    return jsonify({
        "success": True,
        "trained": retrained,
        "samples_processed": len(processed_ids),
        "results": results[-3:] if results else [],  # Last 3 results
        "current_stats": get_training_stats()
    })

# =========================
# HELPERS
# =========================
def preprocess_image(file):
    file.stream.seek(0)
    img = Image.open(file.stream).convert("RGB").resize(IMG_SIZE)
    arr = np.asarray(img, dtype=np.float32) / 255.0
    return np.expand_dims(arr, axis=0)

# =========================
# RUN
# =========================
if __name__ == "__main__":
    print("🌾 AgriSmart ML Server")
    print(f"   Database: {DB_PATH}")
    print(f"   Auto-retrain: Enabled (threshold: 10 samples)")
    app.run(host="0.0.0.0", port=5000, debug=True, threaded=True)
