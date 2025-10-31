import pandas as pd
from sklearn.ensemble import IsolationForest
import joblib

# 1. Load the dataset
df = pd.read_csv("normal_machine_data.csv")

# 2. Select features (no label needed)
X_train = df[["hour", "vibration", "temperature_c"]]

# 3. Train the Isolation Forest model
model = IsolationForest(
    n_estimators=100,
    contamination="auto",   # <-- fixed here
    random_state=42
)
model.fit(X_train)

# 4. Save the trained model
joblib.dump(model, "isolation_forest_model.pkl")

print("✅ Model trained and saved as isolation_forest_model.pkl")
