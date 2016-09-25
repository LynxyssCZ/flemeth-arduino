#include <LiquidCrystal.h>
#include "DS18B20.h"

#define LCD_UPDATE_PERIOD 600U
#define DS_UPDATE_PERIOD 60000UL
#define KEEP_ALIVE 120000UL

#define ONE_WIRE_BUS 13
#define SERIAL_BAUD  115200
#define DRF_BAUD 9600

#define FLEM_REMOTE "@CRP"
#define FLEM_LOCAL "@CLP"
#define FLEM_END "/\n"

#define DRF_EN A5
#define DRF_SIZE 6
#define RELAY 12

#define LCD_RS 7
#define LCD_EN 6
#define LCD_D4 4
#define LCD_D5 5
#define LCD_D6 2
#define LCD_D7 3
#define LCD_RW 8
#define LCD_BL 9

// Loop magic
unsigned int lcdCycle = 0;
unsigned long pingCycle = 0;
unsigned int dsCycle = 0;
bool updateLCDFlag = FALSE;
bool readDSFlag = FALSE;

// Remote magic
bool remoteReceiving = FALSE;
char remoteCommand;
int payloadSize;

// State magic
bool switchState = FALSE;
bool remotePresent = FALSE;

// Basic setup
DS18B20 dallas(ONE_WIRE_BUS, DS_RES_12);
LiquidCrystal lcd(LCD_RS, LCD_RW, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

void setup() {
	// Timer0 is already used for millis() - we'll just interrupt somewhere
	// in the middle and call the "Compare A" function below
	OCR0A = 0xAF;
	TIMSK0 |= _BV(OCIE0A);

	pinMode(LCD_BL, OUTPUT);
	pinMode(RELAY, OUTPUT);
	pinMode(LED_BUILTIN, OUTPUT);
	analogWrite(LCD_BL, 255);

	pinMode(DRF_EN, OUTPUT);
	digitalWrite(DRF_EN, FALSE);

	lcd.begin(16, 2);
	lcd.noBlink();
	Serial.begin(SERIAL_BAUD);
	Serial1.begin(DRF_BAUD);
	dallas.begin();
}

void loop() {
	if (digitalRead(RELAY) != switchState) {
		digitalWrite(RELAY, switchState);
		digitalWrite(LED_BUILTIN, switchState);
	}

	if (!dallas.isReading() && readDSFlag) {
		dallas.startRead();
		readDSFlag = FALSE;
	}

	if (dallas.isReading() && dallas.isReady()) {
		dallas.readTemp();
		reportDallasTemp();
	}

	if (updateLCDFlag) {
		updateLCD();
		updateLCDFlag = FALSE;
	}

	if (Serial1.available() >= DRF_SIZE) {
		processDRFPacket();
	}

	if (Serial.available()) {
		processRemote();
	}
}

void processDRFPacket() {
	byte data[DRF_SIZE];

	while (Serial1.available() >= DRF_SIZE) {
		Serial1.readBytes(data, DRF_SIZE);

		Serial.write(FLEM_REMOTE);
		Serial.print(String(DRF_SIZE));
		Serial.write(data, 6);
		Serial.write(FLEM_END);
	}
}

void reportDallasTemp() {
	byte data[2];

	dallas.getRaw(data);

	Serial.write(FLEM_LOCAL);
	Serial.print("2");
	Serial.write(data, 2);
	Serial.write(FLEM_END);
}

void processRemote() {
	byte remotePayload[10];
	bool commandReceived = FALSE;

	if (Serial.peek() == '*') {
		while (Serial.available()) {
			Serial.read();
		}
		resetRemoteState();
		return;
	}

	if (!remoteReceiving && Serial.available() >= 5) {
		// Wait for fixed size preable @C_PL
		if (Serial.read() != '@') {
			return;
		}

		if (Serial.read() != 'C') {
			return;
		}

		remoteCommand = Serial.read();
		Serial.read(); // Payload
		payloadSize = Serial.read() - 48;
		remoteReceiving = TRUE;
	}
	else if (remoteReceiving && Serial.available() >= (payloadSize + 2)) {
		// If we already started receiving, wait for specified size
		Serial.readBytes(remotePayload, payloadSize);
		Serial.read();
		Serial.read();
		commandReceived = TRUE;
	}

	if (commandReceived) {
		switch (remoteCommand) {
			case 'S':
				// Switch command @CSP1I/
				switchState = remotePayload[0] == 'I';
				break;
		}

		resetRemoteState();
	}
}

void resetRemoteState() {
	remoteCommand = 0x0;
	payloadSize = 0;
	remoteReceiving = FALSE;
	remotePresent = TRUE;
	pingCycle = 0;
}

void updateLCD() {
	float celsius = dallas.getTemp();
	lcd.clear();
	lcd.home();

	if (remotePresent) {
		lcd.print(pingCycle);
	}
	else {
		lcd.print("Waiting...");
		lcd.setCursor(0, 1);

		lcd.print("Temp: ");

		if (celsius) {
			lcd.print(celsius, 2);
		}
		else {
			lcd.print("##.##");
		}
	}

	lcd.setCursor(15, 0);
	lcd.print(switchState ? "1" : "0");
}

// Check flags here
ISR(TIMER0_COMPA_vect) {
	if (lcdCycle % LCD_UPDATE_PERIOD == 0) {
		updateLCDFlag = TRUE;
		lcdCycle = 0;
	}

	if (dsCycle % DS_UPDATE_PERIOD == 0) {
		readDSFlag = TRUE;
		dsCycle = 0;
	}

	if (pingCycle >= KEEP_ALIVE) {
		remotePresent = FALSE;
		switchState = FALSE;
		pingCycle = 0;
	}
	else if (remotePresent) {
		pingCycle++;
	}

	lcdCycle++;
	dsCycle++;
}