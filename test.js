///const addon = require('bindings')('linux_sound_capture_utility');
//const addon = require('./index/soundCaptureUtility');
//import soundCaptureUtility from './index/soundCaptureUtility';

const soundCaptureUtility = require('bindings')('linux_sound_capture_utility');
const addon = new soundCaptureUtility.SoundCaptureUtility(); 
const eventEmitter = require('events').EventEmitter;

const emitter = new eventEmitter();
emitter.on('data', (encoded_audio, pts) => {
    console.log(encoded_audio.length);
    console.log(pts);
});

console.log('starting listener');
addon.startListener(emitter.emit.bind(emitter));

setTimeout(() => {
    console.log('stopping listener');
    addon.closeListener();
}, 5000);
