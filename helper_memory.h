// Debugging helper - shows free heap memory
void display_freeram() {
    Serial.print("- SRAM left: ");
    Serial.println(ESP.getFreeHeap());
}