// Use the newer ALSA API
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>
extern "C"
{
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
}
#include <stdio.h>
#include <stdint.h>


int init_capturer(snd_pcm_t **handle, snd_pcm_uframes_t frames, char **buffer, int *size) {
    int err;
    snd_pcm_hw_params_t *params;
    int dir;
    char *device = (char*) "default";

    // Settings
    unsigned int sample_rate = 44100; // CD Quality
    unsigned int bits_per_sample = 16; // As we are using S16_LE forma
    unsigned int number_of_channels = 2; // stereo
    uint32_t duration = 5000; // duration to record in milliseconds

    printf("Capture device is %s\n", device);

    /* Open PCM device for recording (capture). */
    err = snd_pcm_open(handle, device, SND_PCM_STREAM_CAPTURE, 0);
    if (err)
    {
        fprintf(stderr, "Unable to open PCM device: %s\n", snd_strerror(err));
        return err;
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(*handle, params);

    /* ### Set the desired hardware parameters. ### */

    /* Interleaved mode */
    err = snd_pcm_hw_params_set_access(*handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err)
    {
        fprintf(stderr, "Error setting interleaved mode: %s\n", snd_strerror(err));
        snd_pcm_close(*handle);
        return err;
    }

    /* Signed capture format (16-bit little-endian format) */
    if (bits_per_sample == 16) err = snd_pcm_hw_params_set_format(*handle, params, SND_PCM_FORMAT_S16_LE);
    else err = snd_pcm_hw_params_set_format(*handle, params, SND_PCM_FORMAT_U8);
    if (err)
    {
        fprintf(stderr, "Error setting format: %s\n", snd_strerror(err));
        snd_pcm_close(*handle);
        return err;
    }

    /* Setting number of channels */
    err = snd_pcm_hw_params_set_channels(*handle, params, number_of_channels);
    if (err)
    {
        fprintf(stderr, "Error setting channels: %s\n", snd_strerror(err));
        snd_pcm_close(*handle);
        return err;
    }

    /* Setting sampling rate */
    err = snd_pcm_hw_params_set_rate_near(*handle, params, &sample_rate, &dir);
    if (err)
    {
        fprintf(stderr, "Error setting sampling rate (%d): %s\n", sample_rate, snd_strerror(err));
        snd_pcm_close(*handle);
        return err;
    }

    /* Set period size*/
    err = snd_pcm_hw_params_set_period_size_near(*handle, params, &frames, &dir);
    if (err)
    {
        fprintf(stderr, "Error setting period size: %s\n", snd_strerror(err));
        snd_pcm_close(*handle);
        return err;
    }

    /* Write the parameters to the driver */
    err = snd_pcm_hw_params(*handle, params);
    if (err < 0)
    {
        fprintf(stderr, "Unable to set HW parameters: %s\n", snd_strerror(err));
        snd_pcm_close(*handle);
        return err;
    }

    /* Use a buffer large enough to hold one period (Find number of frames in one period) */
    err = snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    if (err)
    {
        fprintf(stderr, "Error retrieving period size: %s\n", snd_strerror(err));
        snd_pcm_close(*handle);
        return err;
    }

    /* Allocating buffer in number of bytes per period (2 bytes/sample, 2 channels) */
    *size = frames * bits_per_sample / 8 * number_of_channels; 
    *buffer = (char *) malloc(*size);
    if (!buffer)
    {
        fprintf(stdout, "Buffer error.\n");
        snd_pcm_close(*handle);
        return -1;
    }

    unsigned int period_time;
    err = snd_pcm_hw_params_get_period_time(params, &period_time, &dir);
    if (err)
    {
        fprintf(stderr, "Error retrieving period time: %s\n", snd_strerror(err));
        snd_pcm_close(*handle);
        free(buffer);
        return err;
    }

    unsigned int bytes_per_frame = bits_per_sample / 8 * number_of_channels;
    uint32_t pcm_data_size = period_time * bytes_per_frame * duration / 1000;

    printf("Sample rate: %d Hz\n", sample_rate);
    printf("Channels: %d\n", number_of_channels);
    printf("Duration: %d millisecs\n", duration);
    printf("Number of frames: %d", frames);
}

void close_capturer(snd_pcm_t **handle, char** buffer) {
    snd_pcm_drain(*handle);
    snd_pcm_close(*handle);
    free(*buffer);
}

int init_resampler(struct SwrContext **swr_ctx, 
                    int *src_nb_samples, uint8_t ***src_data,
                    int *dst_nb_samples, uint8_t ***dst_data) {
    int64_t src_ch_layout = AV_CH_LAYOUT_STEREO;
    int src_rate = 44100;
    enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_S16;
    
    int64_t dst_ch_layout = AV_CH_LAYOUT_STEREO;
    int dst_rate = 44100;
    enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_FLTP;

    int ret;

    int src_nb_channels = 0;
    int src_linesize;

    int max_dst_nb_samples;
    int dst_nb_channels = 0;
    int dst_linesize;

    /* create resampler context */
    *swr_ctx = swr_alloc();
    if (!*swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        return -1;
    }

    /* set options */
    av_opt_set_int(*swr_ctx, "in_channel_layout",    src_ch_layout, 0);
    av_opt_set_int(*swr_ctx, "in_sample_rate",       src_rate, 0);
    av_opt_set_sample_fmt(*swr_ctx, "in_sample_fmt", src_sample_fmt, 0);

    av_opt_set_int(*swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
    av_opt_set_int(*swr_ctx, "out_sample_rate",       dst_rate, 0);
    av_opt_set_sample_fmt(*swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(*swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        return -1;
    }

    /* allocate source and destination samples buffers 
        - For S16, 2 channels 16 bits per sample. So, each sample takes 4 bytes.
        - Linesize is the total bytes allocated in multiples of 128. 
        - Based on src_nb_samples, src_linesize is decided and space allocated in src_data
    */

    src_nb_channels = av_get_channel_layout_nb_channels(src_ch_layout);
    ret = av_samples_alloc_array_and_samples(src_data, &src_linesize, src_nb_channels,
                                             *src_nb_samples, src_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        return -1;
    }

    printf("Line size : %d\n", src_linesize);

    /* compute the number of converted samples: buffering is avoided
     * ensuring that the output buffer will contain at least all the
     * converted input samples */
    max_dst_nb_samples = *dst_nb_samples =
        av_rescale_rnd(*src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);

    printf("Number of samples in dest: %d\n", *dst_nb_samples);

    /* buffer is going to be directly written to a rawaudio file, no alignment */
    dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
    ret = av_samples_alloc_array_and_samples(dst_data, &dst_linesize, dst_nb_channels,
                                             *dst_nb_samples, dst_sample_fmt, 0);
}

/* check that a given sample format is supported by the encoder */
static int check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt)
{
    const enum AVSampleFormat *p = codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        printf("%d\n", *p);
        if (*p == sample_fmt)
            return 1;
        p++;
    }
    return 0;
}
/* just pick the highest supported samplerate */
static int select_sample_rate(const AVCodec *codec)
{
    const int *p;
    int best_samplerate = 0;
    if (!codec->supported_samplerates)
        return 44100;
    p = codec->supported_samplerates;
    while (*p) {
        if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate))
            best_samplerate = *p;
        p++;
    }
    return best_samplerate;
}
/* select layout with the highest channel count */
static int select_channel_layout(const AVCodec *codec)
{
    const uint64_t *p;
    uint64_t best_ch_layout = 0;
    int best_nb_channels   = 0;
    if (!codec->channel_layouts)
        return AV_CH_LAYOUT_STEREO;
    p = codec->channel_layouts;
    while (*p) {
        int nb_channels = av_get_channel_layout_nb_channels(*p);
        if (nb_channels > best_nb_channels) {
            best_ch_layout    = *p;
            best_nb_channels = nb_channels;
        }
        p++;
    }
    return best_ch_layout;
}

void open_encoder(AVCodecContext **c, AVFrame **frame, AVPacket **pkt) {
    const AVCodec *codec;
    int ret;
    
    avcodec_register_all();
    /* find the MP2 encoder */
    codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
    *c = avcodec_alloc_context3(codec);
    if (!*c) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }

    /* put sample parameters */
    (*c)->bit_rate = 64000;
    /* check that the encoder supports s16 pcm input */
    (*c)->sample_fmt = AV_SAMPLE_FMT_S16;
    if (!check_sample_fmt(codec, (*c)->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s",
                av_get_sample_fmt_name((*c)->sample_fmt));
        exit(1);
    }
    /* select other audio parameters supported by the encoder */
    (*c)->sample_rate    = select_sample_rate(codec);
    (*c)->channel_layout = select_channel_layout(codec);
    (*c)->channels       = av_get_channel_layout_nb_channels((*c)->channel_layout);
    /* open it */
    if (avcodec_open2(*c, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }
    /* packet for holding encoded output */
    *pkt = av_packet_alloc();
    if (!*pkt) {
        fprintf(stderr, "could not allocate the packet\n");
        exit(1);
    }
    /* frame containing input raw audio */
    *frame = av_frame_alloc();
    if (!*frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        exit(1);
    }
    (*frame)->nb_samples     = (*c)->frame_size;
    (*frame)->format         = (*c)->sample_fmt;
    (*frame)->channel_layout = (*c)->channel_layout;

    /* allocate the data buffers */
    ret = av_frame_get_buffer(*frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate audio data buffers\n");
        exit(1);
    }
}

static void encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt,
                   FILE *output)
{
    int ret;
    /* send the frame for encoding */
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        exit(1);
    }
    /* read all the available output packets (in general there may be any
     * number of them */
    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            exit(1);
        }
        printf("Size of pkt : %d\n", pkt->size);
        fwrite(pkt->data, 1, pkt->size, output);
        av_packet_unref(pkt);
    }
}

int main(int argc, char *argv[]) {
    snd_pcm_t *handle;
    snd_pcm_uframes_t frames = 512;
    int size;
    char *buffer;
    int err;
    int filedesc;
    int filedesc_resampled;

    /* Resampling related */
    struct SwrContext *swr_ctx;

    uint8_t **src_data = NULL;
    int src_nb_samples = frames;

    uint8_t **dst_data = NULL;
    int dst_nb_samples;

    int ret;
    /* Resampling related end */

    /* Encoder related */
    AVCodecContext *c= NULL;
    AVFrame *frame;
    AVPacket *pkt;
    /* Encoder related end */

    // Read file name
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s (output file name)\n", argv[0]);
        return -1;
    }
    char* fileName = argv[1];

    filedesc = open(fileName, O_WRONLY | O_CREAT, 0644);

    strcat(fileName, "_res");
    filedesc_resampled = open(fileName, O_WRONLY | O_CREAT, 0644);

    strcat(fileName, ".aac");
    FILE *file_enc = fopen(fileName, "wb");

    err = init_capturer(&handle, frames, &buffer, &size);
    
    init_resampler(&swr_ctx, &src_nb_samples, &src_data, &dst_nb_samples, &dst_data);
    printf("Buffer size allocated : %d\n", size);

    open_encoder(&c, &frame, &pkt);

    for(int i = 0; i < 1000; i++)
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
        memcpy(src_data[0], buffer, size);
        ret = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)src_data, src_nb_samples);
        // printf("Number of samples converted %d\n", ret);
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            return -1;
        }

        write(filedesc_resampled, dst_data[0], size*2);

        /* make sure the frame is writable -- makes a copy if the encoder
         * kept a reference internally */
        ret = av_frame_make_writable(frame);
        if (ret < 0)
            exit(1);

        frame->data[0] = dst_data[0];

        encode(c, frame, pkt, file_enc);
    }
    close(filedesc);
    close(filedesc_resampled);
    close_capturer(&handle, &buffer);

    if (src_data)
        av_freep(&src_data[0]);
    av_freep(&src_data);

    if (dst_data)
        av_freep(&dst_data[0]);
    av_freep(&dst_data);

    swr_free(&swr_ctx);

    /* flush the encoder */
    encode(c, NULL, pkt, file_enc);
    fclose(file_enc);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&c);
}