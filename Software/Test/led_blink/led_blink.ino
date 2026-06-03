void setup() {
  // ATmega8-ന്റെ ഫിസിക്കൽ Pin 14 (PB0) ആണ് MiniCore പ്രകാരം ഡിജിറ്റൽ പിൻ 8
  pinMode(8, OUTPUT); 
}

void loop() {
  digitalWrite(8, HIGH); // Segment A ഓൺ ആകുന്നു
  delay(2000);           // 1 സെക്കൻഡ് വെയിറ്റ് ചെയ്യുന്നു
  digitalWrite(8, LOW);  // Segment A ഓഫ് ആകുന്നു
  delay(1000);           // 1 സെക്കൻഡ് വെയിറ്റ് ചെയ്യുന്നു
}