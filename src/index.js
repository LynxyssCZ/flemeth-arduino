'use strict';
const SerialPort = require('serialport');

class FlemDuino {
	constructor(app, options) {
		this.app = app;
		this.logger = app.logger.child({component: 'FlemDuino'});
		this.flux = app.methods.flux;

		this.serialPath = options.path;

		this.app.addHook('lifecycle.start', this.onAppStart.bind(this));
		this.app.addHook('lifecycle.stop', this.onAppStop.bind(this));

		this.serialPort = new SerialPort(options.path, {
			baudrate: 115200,
			parser: SerialPort.parsers.raw,
			autoOpen: false
		});
		this.serialPort.on('data', this.onSerialRead.bind(this));
	}

	// Hooks
	onAppStart(payload, next) {
		this.logger.info('Starting');

		this.serialPort.open((err) => {
			this.subscriptionKey = this.flux.subscribe(this.update.bind(this), [
				'ZonesMean', 'ScheduleTarget', 'Switcher'
			]);

			next(err);
		});
	}

	onAppStop(payload, next) {
		this.logger.info('Stopping');

		this.flux.unsubscribe(this.subscriptionKey);
		if (this.serialPort.isOpen) {
			this.serialPort.close((err) => {
				next(err);
			});
		}
		else {
			next();
		}
	}

	update() {
		const switcher = this.flux.getSlice('Switcher').get('realValue');
		const current = this.flux.getSlice('ZonesMean').get('temperature');
		const target = this.flux.getSlice('ScheduleTarget').get('temperature');
		let command = '';

		if (switcher) {
			command = command.concat('@CSP01I/\n');
		}
		else {
			command = command.concat('@CSP01O/\n');
		}

		if (current) {
			command = command.concat('@CCP02' + this.tempToBytes(current) + '/\n');
		}

		if (target) {
			command = command.concat('@CTP02' + this.tempToBytes(target) + '/\n');
		}

		if (this.serialPort.isOpen) {
			this.serialPort.write(command);
		}
	}

	tempToBytes(value) {
		let buffer = Buffer.allocUnsafe(2);
		buffer.writeUInt16LE(Math.round(value * 16), 0);
		return buffer.toString('binary');
	}

	onSerialRead(data) {
		if (data[0] === 0x40 && data[1] === 0x43) { // Command
			let payload = data.slice(6, data.length - 2);
			let samples = [];

			switch (data[2]) {
				case 0x4c: // Local
					samples = [{
						sensorId: 'LocalDS',
						type: 'temp',
						value: (payload.readUInt16LE(0) / 16),
						time: Date.now()
					}];
					break;
				case 0x52: // Remote
					samples = [{
						sensorId: 'DRF5150-' + payload.readUInt8(0) + '-' + payload.readUInt8(1),
						type: 'temp',
						value: (payload.readUInt16LE(2) / 16),
						meta: {
							vbat: (payload.readUInt8(4) / 100) + 2,
							rssi: payload.readUInt8(5)
						},
						time: Date.now()
					}];
					break;
			}

			if (samples) {
				this.app.methods.sensors.readFrame({
					reader: 'FlemDuino',
					samples: samples
				});
			}

			this.serialPort.write([0x2a, 0x0a]);
		}
	}
}
module.exports = FlemDuino;
