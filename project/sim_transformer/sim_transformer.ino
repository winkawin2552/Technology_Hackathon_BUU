int ENA = 9;  // PWM pin
int IN1 = 7;
int IN2 = 8;

int speedValue = 0;

void setup() {
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  Serial.begin(9600);
  Serial.println("Type a number (0–255) to set motor speed:");
  
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');  // read until Enter
    input.trim();  // remove spaces/newlines

    if (input.length() > 0) {  // only process if not empty
      int value = input.toInt();
      value = constrain(value, 0, 255);
      analogWrite(ENA, value);

      Serial.print("Motor speed set to: ");
      Serial.println(value);
    }
  }
}
