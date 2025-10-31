import pandas as pd
import joblib

# 1. Load the trained model
model = joblib.load("isolation_forest_model.pkl")

# 2. Load or create new test data
# (You can also replace this with pd.read_csv("new_data.csv"))
test_data = pd.DataFrame({
    "hour": [2, 9, 14, 15, 21],
    "vibration": [1190, 1250, 1450, 1100, 1350],  # one or two abnormal values
    "temperature_c": [24.5, 25.5, 31.2, 29.0, 25.8]
})

# 3. Predict using the model
predictions = model.predict(test_data)  # 1 = normal, -1 = anomaly
scores = model.decision_function(test_data)  # higher = more normal

# 4. Calculate anomaly percentage
num_anomalies = (predictions == -1).sum()
total = len(predictions)
anomaly_percent = (num_anomalies / total) * 100

# 5. Display results
test_data["prediction"] = predictions
test_data["anomaly_score"] = scores

print("🔍 Test Results:")
print(test_data)
print(f"\n⚠️  Detected {num_anomalies} anomalies out of {total} samples "
      f"({anomaly_percent:.2f}% abnormal)")

