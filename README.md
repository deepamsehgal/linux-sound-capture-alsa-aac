# Linux sound capture programs

This repository contains various programs to capture sound that is being played through the speakers in Linux.

We use ALSA libraries to capture the sound in raw PCM format.

We then use ffmpeg libraries to resample and encode the raw sound data to AAC format.

## alsa-record.cpp

```
g++ alsa-record -o alsa-record -lasound
./alsa-record <filename>
```
Raw PCM data recorded in Signed 16 bit little endian, stereo format will be stored in \<filename>.
You can play it using following command
```
aplay <filename> -f cd
```

## alsa-record-wav.cpp
```
g++ alsa-record -o alsa-record -lasound
./alsa-record <filename>
```
Raw PCM data recorded in Signed 16 bit little endian, stereo format will be stored in the .wav format with name as \<filename>.wav
You can play it using following command
```
aplay <filename>.wav
```

