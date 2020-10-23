// Use the newer ALSA API
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    int err;
    int size;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    int dir;
    snd_pcm_uframes_t frames = 32;
    char *device = (char*) "default";
    char *buffer;
    int filedesc;

    // Read file name
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s (output file name)\n", argv[0]);
        return -1;
    }
    char* fileName = argv[1];

    // Settings
    unsigned int sample_rate = 44100; // CD Quality
    unsigned int bits_per_sample = 16; // As we are using S16_LE forma
    unsigned int number_of_channels = 2; // stereo
    uint32_t duration = 5000; // duration to record in milliseconds

    printf("Capture device is %s\n", device);

    /* Open PCM device for recording (capture). */
    err = snd_pcm_open(&handle, device, SND_PCM_STREAM_CAPTURE, 0);
    if (err)
    {
        fprintf(stderr, "Unable to open PCM device: %s\n", snd_strerror(err));
        return err;
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(handle, params);

    /* ### Set the desired hardware parameters. ### */

    /* Interleaved mode */
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err)
    {
        fprintf(stderr, "Error setting interleaved mode: %s\n", snd_strerror(err));
        snd_pcm_close(handle);
        return err;
    }

    /* Signed capture format (16-bit little-endian format) */
    if (bits_per_sample == 16) err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    else err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_U8);
    if (err)
    {
        fprintf(stderr, "Error setting format: %s\n", snd_strerror(err));
        snd_pcm_close(handle);
        return err;
    }

    /* Setting number of channels */
    err = snd_pcm_hw_params_set_channels(handle, params, number_of_channels);
    if (err)
    {
        fprintf(stderr, "Error setting channels: %s\n", snd_strerror(err));
        snd_pcm_close(handle);
        return err;
    }

    /* Setting sampling rate */
    err = snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, &dir);
    if (err)
    {
        fprintf(stderr, "Error setting sampling rate (%d): %s\n", sample_rate, snd_strerror(err));
        snd_pcm_close(handle);
        return err;
    }

    /* Set period size*/
    err = snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    if (err)
    {
        fprintf(stderr, "Error setting period size: %s\n", snd_strerror(err));
        snd_pcm_close(handle);
        return err;
    }

    /* Write the parameters to the driver */
    err = snd_pcm_hw_params(handle, params);
    if (err < 0)
    {
        fprintf(stderr, "Unable to set HW parameters: %s\n", snd_strerror(err));
        snd_pcm_close(handle);
        return err;
    }

    /* Use a buffer large enough to hold one period (Find number of frames in one period) */
    err = snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    if (err)
    {
        fprintf(stderr, "Error retrieving period size: %s\n", snd_strerror(err));
        snd_pcm_close(handle);
        return err;
    }

    /* Allocating buffer in number of bytes per period (2 bytes/sample, 2 channels) */
    size = frames * bits_per_sample / 8 * number_of_channels; 
    buffer = (char *) malloc(size);
    if (!buffer)
    {
        fprintf(stdout, "Buffer error.\n");
        snd_pcm_close(handle);
        return -1;
    }

    unsigned int period_time;
    err = snd_pcm_hw_params_get_period_time(params, &period_time, &dir);
    if (err)
    {
        fprintf(stderr, "Error retrieving period time: %s\n", snd_strerror(err));
        snd_pcm_close(handle);
        free(buffer);
        return err;
    }

    unsigned int bytes_per_frame = bits_per_sample / 8 * number_of_channels;
    uint32_t pcm_data_size = period_time * bytes_per_frame * duration / 1000;

    filedesc = open(fileName, O_WRONLY | O_CREAT, 0644);

    printf("Sample rate: %d Hz\n", sample_rate);
    printf("Channels: %d\n", number_of_channels);
    printf("Duration: %d millisecs\n", duration);

    for(int i = duration * 1000 / period_time; i > 0; i--)
    {
        err = snd_pcm_readi(handle, buffer, frames);
        // if (err == -EPIPE) fprintf(stderr, "Overrun occurred: %d\n", err);
        // if (err) err = snd_pcm_recover(handle, err, 0);
        // Still an error, need to exit.
        if (err <= 0)
        {
            fprintf(stderr, "Error occured while recording: %s\n", snd_strerror(err));
            snd_pcm_close(handle);
            free(buffer);
            close(filedesc);
            return err;
        }
        write(filedesc, buffer, size);
    }

    close(filedesc);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);

    printf("Finished writing to %s\n", fileName);
    return 0;
}