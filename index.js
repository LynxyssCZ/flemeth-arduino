var SerialPort = require('serialport');

var serialPort = new SerialPort('COM7', {
	baudrate: 115200,
	parser: SerialPort.parsers.raw
});

serialPort.on('data', (data) => {
	if (data[0] === 0x40 && data[1] === 0x43) { // Command
		let payload = data.slice(6, data.length - 2);
		console.log(payload);
		switch (data[2]) {
			case 0x4c: // Local
				console.log('DS18b20');
				console.log('Temp: ', (payload.readUInt16LE(0) / 16));
				break;
			case 0x52: // Remote
				console.log('DRF', payload.readUInt8(0) + '-' + payload.readUInt8(1));
				console.log('Temp:', (payload.readUInt16LE(2) / 16), 'VBAT:', (payload.readUInt8(4) / 100) + 2, 'RSSI:', payload.readUInt8(5));
				var buffer = Buffer.allocUnsafe(2);
				buffer.writeUInt16LE((payload.readUInt16LE(2) / 16) * 16);
				console.log(buffer);
				console.log(buffer.toString());
				break;
		}

		serialPort.write([0x2a, 0x0a]);
	}


});
