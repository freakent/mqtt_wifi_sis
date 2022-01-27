void debug_debounce(String label) {
  Serial.print(label);
  Serial.print(" LDT:");
  Serial.print(lastDebounceTime);
  Serial.print(" LBR:");
  Serial.print(lastButtonReading);
  Serial.print(" BR:");
  Serial.print(buttonReading);
  Serial.print(" BS:");
  Serial.println(buttonState);
}