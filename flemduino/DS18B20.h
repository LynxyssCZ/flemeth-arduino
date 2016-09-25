#ifndef DS18B20_h
#define DS18B20_h
#include <OneWire.h>

// Config bytes
#define DS_RES_9 0x1F
#define DS_RES_10 0x2F
#define DS_RES_11 0x3F
#define DS_RES_12  0x7F

#define DS_DEFAULT_PRECISION DS_RES_10
// DS commands
#define DS_COMMAND_CONVERT	0x44
#define DS_COMMAND_WRITE	0x4E
#define DS_COMMAND_READ		0xBE
#define DS_COMMAND_COPY		0x48


class DS18B20 {
	public:
		DS18B20(int);
		DS18B20(int, byte);
		// Sets the precission
		void setPrecission(byte);
		// Sets up the DS for transmission
		bool begin(void);
		// Read address of the DS18B20
		void getAddress(byte[]);
		// Is reading
		bool isReading(void);
		// Is ready
		bool isReady(void);
		// Starts the DS read cycle
		void startRead(void);
		// get raw DS temp
		void getRaw(byte[]);
		// Reads the Temperature from DS
		float readTemp();
		// Gets cached temp
		float getTemp();

	private:
		OneWire _bus;
		byte _address[8];
		byte _data[2];
		byte _precission;
		float celsius = 0.0;
		bool reading = false;

		bool _findFirstDS();
		// Fetces the resulting temperature from DS
		float _fetchDSTemp();
};

#endif
