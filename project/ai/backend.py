from flask import Flask, request, jsonify
import joblib
import pandas as pd

model = joblib.load("isolation_forest_model.pkl")
app = Flask(__name__)

@app.route('/predict', methods=['POST'])
def predict():
    data = request.get_json()
    df = pd.DataFrame([{
        "hour": data.get("hour"),
        "vibration": data.get("vibration"),
        "temperature_c": data.get("temperature_c")
    }])
    prediction = int(model.predict(df)[0])
    score = float(model.decision_function(df)[0])
    return jsonify({"prediction": prediction, "anomaly_score": score})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
