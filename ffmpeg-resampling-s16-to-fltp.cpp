extern "C"
{
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <stdio.h>
#include <stdint.h>

int main(int argc, char** argv) {
    struct SwrContext *swr_ctx;

    int64_t src_ch_layout = AV_CH_LAYOUT_STEREO;
    int src_rate = 44100;
    enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_S16;
    
    int64_t dst_ch_layout = AV_CH_LAYOUT_STEREO;
    int dst_rate = 44100;
    enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_FLT;

    int ret;

    int src_nb_channels = 0;
    int src_linesize;
    uint8_t **src_data = NULL;
    int src_nb_samples = 7000*32; // Vary based on number of samples in input

    int dst_nb_samples, max_dst_nb_samples;
    int dst_nb_channels = 0;
    int dst_linesize;
    uint8_t **dst_data = NULL;

    char* src_filename = NULL;
    FILE *src_file;
    int src_bufsize;

    char* dst_filename = NULL;
    FILE *dst_file;
    int dst_bufsize;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s input_file output_file\n"
                "API example program to show how to resample an audio stream with libswresample.\n"
                "This program generates a series of audio frames, resamples them to a specified "
                "output format and rate and saves them to an output file named output_file.\n",
            argv[0]);
        exit(1);
    }

    src_filename = argv[1];
    src_file = fopen(src_filename, "r");
    if (!src_file) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        exit(1);
    }

    dst_filename = argv[2];

    dst_file = fopen(dst_filename, "wb");
    if (!dst_file) {
        fprintf(stderr, "Could not open destination file %s\n", dst_filename);
        exit(1);
    }

    /* create resampler context */
    swr_ctx = swr_alloc();
    if (!swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        return -1;
    }

    /* set options */
    av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate",       src_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);

    av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate",       dst_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        return -1;
    }

    /* allocate source and destination samples buffers 
        - For S16, 2 channels 16 bits per sample. So, each sample takes 4 bytes.
        - Linesize is the total bytes allocated in multiples of 128. 
        - Based on src_nb_samples, src_linesize is decided and space allocated in src_data
    */

    src_nb_channels = av_get_channel_layout_nb_channels(src_ch_layout);
    ret = av_samples_alloc_array_and_samples(&src_data, &src_linesize, src_nb_channels,
                                             src_nb_samples, src_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        return -1;
    }

    printf("Line size : %d\n", src_linesize);

    fread(src_data[0], 1, src_linesize, src_file);

    /* compute the number of converted samples: buffering is avoided
     * ensuring that the output buffer will contain at least all the
     * converted input samples */
    max_dst_nb_samples = dst_nb_samples =
        av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);

    printf("Number of samples in dest: %d\n", dst_nb_samples);

    /* buffer is going to be directly written to a rawaudio file, no alignment */
    dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
    ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
                                             dst_nb_samples, dst_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate destination samples\n");
        return -1;
    }

    ret = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)src_data, src_nb_samples);
    printf("Number of samples converted %d\nDest Line size %d\n", ret, dst_linesize);
    if (ret < 0) {
        fprintf(stderr, "Error while converting\n");
        return -1;
    }

    dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels, ret, dst_sample_fmt, 1);

    if (dst_bufsize < 0) {
        fprintf(stderr, "Could not get sample buffer size\n");
        return -1;
    }
    
    printf("in:%d out:%d bufSize:%d\n", src_nb_samples, ret, dst_bufsize);

    fwrite(dst_data[0], 1, dst_bufsize, dst_file);

    fprintf(stderr, "Resampling succeeded. Play the output file with the command:\n"
        "aplay --format=FLOAT_LE -r %d -c %d %s\n",
        dst_rate, dst_nb_channels, dst_filename);

    fclose(src_file);
    fclose(dst_file);
    
    if (src_data)
        av_freep(&src_data[0]);
    av_freep(&src_data);

    if (dst_data)
        av_freep(&dst_data[0]);
    av_freep(&dst_data);

    swr_free(&swr_ctx);

    return 0;
}