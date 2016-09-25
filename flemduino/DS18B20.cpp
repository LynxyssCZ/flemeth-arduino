#include <OneWire.h>
#include "DS18B20.h"

DS18B20::DS18B20 (int busPin): _bus(busPin), _precission(DS_DEFAULT_PRECISION) {}
DS18B20::DS18B20 (int busPin, byte precission): _bus(busPin), _precission(precission) {}

bool DS18B20::begin() {
	byte present = _bus.reset();
	bool foundDS;

	if (present) {
		foundDS = _findFirstDS();
		return foundDS;
	}
	else {
		return false;
	}
}

void DS18B20::getAddress(byte addr[]) {
	for (byte i = 0; i < 8; ++i) {
		addr[i] = _address[i];
	}
}

bool DS18B20::isReading() {
	return reading;
}

void DS18B20::startRead() {
	// Prepare the DS
	_bus.reset();
	_bus.select(_address);

	_bus.write(DS_COMMAND_WRITE);
	_bus.write(0);
	_bus.write(0);
	_bus.write(_precission);
	_bus.write(DS_COMMAND_COPY);

	_bus.reset();
	_bus.select(_address);
	_bus.write(DS_COMMAND_CONVERT);
	reading = TRUE;
}

bool DS18B20::isReady() {
	return _bus.read();
}

float DS18B20::readTemp() {
	unsigned int raw;

	reading = false;
	_bus.reset();
	_bus.select(_address);
	_bus.write(DS_COMMAND_READ);
	_data[0] = _bus.read();
	_data[1] = _bus.read();

	raw = (_data[1] << 8) | _data[0];

	celsius = (float)raw / 16.0;

	return celsius;
}

void DS18B20::getRaw(byte temp[]) {
	temp[0] = _data[0];
	temp[1] = _data[1];
}

float DS18B20::getTemp() {
	return celsius;
}

bool DS18B20::_findFirstDS() {
	byte addr[8];
	byte i;
	bool found = false;

	while (_bus.search(addr)) {
		if (OneWire::crc8(addr, 7) != addr[7]) {
			break;
		}
		for(i = 0; i < 8; ++i) {
			_address[i]=addr[i];
		}
		found = true;
		break;
	}

	return found;
}
