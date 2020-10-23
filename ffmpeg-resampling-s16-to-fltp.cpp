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
    enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_FLTP;

    int ret;

    int src_nb_channels = 0;
    int src_linesize;
    uint8_t **src_data = NULL;
    int src_nb_samples = 20; // Vary based on number of samples in input

    int dst_nb_samples, max_dst_nb_samples;
    int dst_nb_channels = 0;
    int dst_linesize;
    uint8_t **dst_data = NULL;

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
}