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
			this.subKey = this.flux.subscribe([
				{handler: this.updateMean.bind(this), slices: ['ZonesMean']},
				{handler: this.updateTarget.bind(this), slices: ['ScheduleTarget']},
				{handler: this.updateSwitcher.bind(this), slices: ['Switcher']}
			]);

			next(err);
		});
	}

	onAppStop(payload, next) {
		this.logger.info('Stopping');

		this.flux.unsubscribe(this.subKey);
		if (this.serialPort.isOpen) {
			this.serialPort.close((err) => {
				next(err);
			});
		}
		else {
			next();
		}
	}

	updateMean() {
		const current = this.flux.getSlice('ZonesMean').get('temperature');

		if (this.serialPort.isOpen && current) {
			const buffer = Buffer.from('@CCP02LH/\n');
			buffer.writeUInt16LE(current * 16, 6);

			this.serialPort.write(buffer);
		}
	}

	updateTarget() {
		const target = this.flux.getSlice('ScheduleTarget');
		const temp = target.get('temperature');
		const hyst = target.get('hysteresis');

		if (this.serialPort.isOpen && temp) {
			const buffer = Buffer.from('@CTP04LHLH/\n');
			buffer.writeUInt16LE(temp * 16, 6);
			buffer.writeUInt16LE((hyst * 16) || 0, 8);

			this.serialPort.write(buffer);
		}
	}

	updateSwitcher() {
		const switcher = this.flux.getSlice('Switcher').get('realValue');

		if (this.serialPort.isOpen) {
			this.serialPort.write(Buffer.from(switcher ? '@CSP01I/\n' : '@CSP01O/\n'));
		}
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
