void checkButton() {
  // read the state of the pushbutton value:
  buttonReading = digitalRead(buttonPin);
  //debug_debounce("A");

  if (buttonReading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ( (millis() - lastDebounceTime) > debounceDelay ) {
    //debug_debounce("B");

    if (buttonState != buttonReading) { 
      buttonState = buttonReading;
      //debug_debounce("C");

      // check if the pushbutton is pressed. If it is, the buttonState is HIGH:
      if (buttonState == HIGH) {
        Serial.println("Button Pressed ");
        digitalWrite(ledPin, HIGH);
        //inverterState = inverterStateTransition[inverterState];
      } else {
        Serial.println("Button Released ");
        digitalWrite(ledPin, LOW);
      }

    } 

  } // if ( ( millis() - lastDebounceTime) > debounceDelay )

  lastButtonReading = buttonReading;
}
