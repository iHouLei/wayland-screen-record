#include "avoutputstream.h"
#include <unistd.h>
#include <QTime>
#include <QDebug>
#include <QThread>

AVOutputStream::AVOutputStream():
    m_pSysAudioSwrContext(nullptr),
    m_micAudioFifo(nullptr),
    m_sysAudioFifo(nullptr)
{
    m_videoCodecID  = AV_CODEC_ID_NONE;
    m_micAudioCodecID = AV_CODEC_ID_NONE;
    m_sysAudioCodecID = AV_CODEC_ID_NONE;
    m_bMix = false;
    m_out_buffer = nullptr;
    m_width = 320;
    m_height = 240;
    m_framerate = 25;
    m_video_bitrate = 500000;
    m_samplerate = 0;
    m_channels = 1;
    m_audio_bitrate = 32000;
    m_samplerate_card = 0;
    is_fifo_scardinit = 0;
    m_channels_card = 1;
    m_audio_bitrate_card = 32000;
    m_videoStream = nullptr;
    m_micAudioStream = nullptr;
    m_videoFormatContext = nullptr;
    pCodecCtx = nullptr;
    m_pMicCodecContext = nullptr;
    pCodec = nullptr;
    pCodec_a = nullptr;
    pCodec_aCard = nullptr;
    pCodec_amix = nullptr;
    pFrameYUV = nullptr;
    m_pVideoSwsContext = nullptr;
    m_pMicAudioSwrContext = nullptr;
    m_nb_samples = 0;
    m_convertedMicSamples = nullptr;
    m_convertedSysSamples = nullptr;
    m_micAudioFrame = 0;
    m_sysAudioFrame = 0;
    m_next_vid_time = 0;
    m_next_aud_time = 0;
    audio_amix_st =nullptr;
    m_nLastAudioPresentationTime = 0;
    m_nLastAudioCardPresentationTime = 0;
    m_nLastAudioMixPresentationTime = 0;
    m_mixCount = 0;
    avcodec_register_all();
    av_register_all();
}

AVOutputStream::~AVOutputStream(void)
{
    printf("Desctruction Onput!\n");
    if (m_micAudioFifo) {
        audioFifoFree(m_micAudioFifo);
        m_micAudioFifo = nullptr;
    }
    if (nullptr != m_sysAudioFifo) {
        audioFifoFree(m_sysAudioFifo);
        m_sysAudioFifo = nullptr;
    }
    if(nullptr != m_path) {
        delete[] m_path;
    }
}

void AVOutputStream::SetVideoCodecProp(AVCodecID codec_id, int framerate, int bitrate, int gopsize, int width, int height)
{
    m_videoCodecID  = codec_id;
    m_width = width;
    m_height = height;
    m_framerate = ((framerate == 0) ? 10 : framerate);
    m_video_bitrate = bitrate;
    m_gopsize = gopsize;
}

void AVOutputStream::SetAudioCodecProp(AVCodecID codec_id, int samplerate, int channels,int layout ,int bitrate)
{
    m_micAudioCodecID = codec_id;
    m_samplerate = samplerate;
    m_channels = channels;
    m_channels_layout = layout;
    m_audio_bitrate = bitrate;
}
void AVOutputStream::SetAudioCardCodecProp(AVCodecID codec_id, int samplerate, int channels, int layout ,int bitrate)
{
    m_sysAudioCodecID = codec_id;
    m_samplerate_card = samplerate;
    m_channels_card = channels;
    m_channels_card_layout = layout;
    m_audio_bitrate_card = bitrate;
}
int AVOutputStream::init_filters()
{
    static const char *filter_descr = "[in0][in1]amix=inputs=2[out]";//"[in0][in1]amix=inputs=2[out]";amerge//"aresample=8000,aformat=sample_fmts=s16:channel_layouts=mono";
    char args1[512];
    char args2[512];
    int ret = 0;
    string formatStr = "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%";
    formatStr.append(PRIx64);
    AVFilterInOut* filter_outputs[2];
    const AVFilter *abuffersrc1  = avfilter_get_by_name("abuffer");
    const AVFilter *abuffersrc2  = avfilter_get_by_name("abuffer");
    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    AVFilterInOut *outputs1 = avfilter_inout_alloc();
    AVFilterInOut *outputs2 = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    static  enum AVSampleFormat out_sample_fmts[] ={ pCodecCtx_amix->sample_fmt, AV_SAMPLE_FMT_NONE }; //{ AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE };
    static const int64_t out_channel_layouts[] = {static_cast<int64_t>(pCodecCtx_amix->channel_layout), -1};
    static const int out_sample_rates[] = { pCodecCtx_amix->sample_rate, -1 };
    const AVFilterLink *outlink;
    AVRational time_base_1 = m_pMicCodecContext->time_base;
    AVRational time_base_2 = m_pSysCodecContext->time_base;
    filter_graph = avfilter_graph_alloc();
    if (!outputs1 || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        avfilter_inout_free(&inputs);

        avfilter_inout_free(&outputs1);
        return 1;
    }
    AVCodecContext *dec_ctx1;
    AVCodecContext *dec_ctx2;
    /* buffer audio source: the decoded frames from the decoder will be inserted here. */
    dec_ctx1 = m_pMicCodecContext;
    if (!dec_ctx1->channel_layout)
        dec_ctx1->channel_layout = static_cast<uint64_t>(av_get_default_channel_layout(dec_ctx1->channels));
    snprintf(args1,
             sizeof(args1),
             formatStr.c_str(),
             time_base_1.num,
             time_base_1.den,
             dec_ctx1->sample_rate,
             av_get_sample_fmt_name(dec_ctx1->sample_fmt),
             dec_ctx1->channel_layout);
    ret = avfilter_graph_create_filter(&buffersrc_ctx1, abuffersrc1, "in0",
                                       args1, nullptr, filter_graph);

    if (ret < 0) {

        av_log(nullptr, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs1);
        return 1;
    }
    dec_ctx2 =m_pSysCodecContext;
    snprintf(args2, sizeof(args2),
             formatStr.c_str(),
             time_base_2.num, time_base_2.den, dec_ctx2->sample_rate,
             av_get_sample_fmt_name(dec_ctx2->sample_fmt), dec_ctx2->channel_layout);
    ret = avfilter_graph_create_filter(&buffersrc_ctx2, abuffersrc2, "in1",
                                       args2, nullptr, filter_graph);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs1);
        return 1;
    }

    ret = avfilter_graph_create_filter(&buffersink_ctx, abuffersink, "out",
                                       nullptr, nullptr, filter_graph);

    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs1);
        return 1;
    }
    ret = av_opt_set_int_list(buffersink_ctx, "sample_fmts", out_sample_fmts, -1,AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot set output sample format\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs1);
        return 1;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "channel_layouts", out_channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);

    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot set output channel layout\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs1);
        return 1;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "sample_rates", out_sample_rates, -1, AV_OPT_SEARCH_CHILDREN);

    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Cannot set output sample rate\n");
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs1);
        return 1;
    }

    outputs1->name       = av_strdup("in0");
    outputs1->filter_ctx = buffersrc_ctx1;
    outputs1->pad_idx    = 0;
    outputs1->next       = outputs2;
    outputs2->name       = av_strdup("in1");
    outputs2->filter_ctx = buffersrc_ctx2;
    outputs2->pad_idx    = 0;
    outputs2->next       = nullptr;
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = nullptr;
    filter_outputs[0] = outputs1;
    filter_outputs[1] = outputs2;
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_descr,
                                        &inputs, filter_outputs, nullptr)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "parse ptr fail, ret: %d\n", ret);
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs1);
        return 1;
    }


    if ((ret = avfilter_graph_config(filter_graph, nullptr)) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "config graph fail, ret: %d\n", ret);
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs1);
        return 1;
    }
    outlink = buffersink_ctx->inputs[0];
    av_get_channel_layout_string(args1, sizeof(args1), -1, outlink->channel_layout);
    av_log(nullptr, AV_LOG_INFO, "Output: srate:%dHz fmt:%s chlayout:%s\n",
           outlink->sample_rate,
           static_cast<char *>(av_x_if_null(av_get_sample_fmt_name(static_cast<AVSampleFormat>(outlink->format)), "?")),
           args1);
    mMic_frame = av_frame_alloc();
    mSpeaker_frame = av_frame_alloc();
    avfilter_inout_free(&inputs);
    avfilter_inout_free(filter_outputs);
    return ret;
}
int AVOutputStream::init_context_amix(int channel, uint64_t channel_layout,int sample_rate,int64_t bit_rate)
{
    Q_UNUSED(channel)
    Q_UNUSED(channel_layout)
    Q_UNUSED(sample_rate)
    Q_UNUSED(bit_rate)
    pCodec_amix = avcodec_find_encoder(m_sysAudioCodecID);

    if (!pCodec_amix)
    {
        return false;
    }

    pCodecCtx_amix = avcodec_alloc_context3(pCodec_amix);
    pCodecCtx_amix = avcodec_alloc_context3(pCodec_a);
    pCodecCtx_amix->channels = m_channels;
    pCodecCtx_amix->channel_layout = static_cast<uint64_t>(m_channels_layout);

    if (pCodecCtx_amix->channel_layout == 0) {
        pCodecCtx_amix->channel_layout = AV_CH_LAYOUT_STEREO;
        pCodecCtx_amix->channels = av_get_channel_layout_nb_channels(pCodecCtx_amix->channel_layout);
    }

    pCodecCtx_amix->sample_rate = m_samplerate;
    pCodecCtx_amix->sample_fmt = pCodec_amix->sample_fmts[0];
    pCodecCtx_amix->bit_rate = m_audio_bitrate;
    pCodecCtx_amix->time_base.num = 1;
    pCodecCtx_amix->time_base.den = pCodecCtx_amix->sample_rate;

    if(m_sysAudioCodecID == AV_CODEC_ID_AAC) {
        pCodecCtx_amix->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    }

    if (m_videoFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
        pCodecCtx_amix->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(pCodecCtx_amix, pCodec_amix, nullptr) < 0) {
        return false;
    }
    audio_amix_st = avformat_new_stream(m_videoFormatContext, pCodec_amix);
    if (audio_amix_st) {
        if (audio_amix_st == nullptr) {
            return false;
        }
        audio_amix_st->time_base.num = 1;
        audio_amix_st->time_base.den = pCodecCtx_amix->sample_rate;//48000
        audio_amix_st->codec = pCodecCtx_amix;
    }
    return true;
}

bool AVOutputStream::open(QString path)
{
    QByteArray pathArry = path.toLocal8Bit();
    m_path = new char[strlen(pathArry.data())+1];
    strcpy(m_path,pathArry.data());
    avformat_alloc_output_context2(&m_videoFormatContext, nullptr, nullptr, m_path);

    if (m_videoCodecID  != 0) {
        pCodec = avcodec_find_encoder(m_videoCodecID );
        if (!pCodec) {
            return false;
        }

        pCodecCtx = avcodec_alloc_context3(pCodec);
        pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
#ifdef VIDEO_RESCALE
        pCodecCtx->width = m_width/2;
        pCodecCtx->height = m_height/2;
#else
        pCodecCtx->width = m_width;
        pCodecCtx->height = m_height;

#endif
        pCodecCtx->time_base.num = 1;
        pCodecCtx->time_base.den = m_framerate;
        pCodecCtx->gop_size = m_gopsize;
        if (m_videoFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
            pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        AVDictionary *param = nullptr;
        if (m_videoCodecID  == AV_CODEC_ID_H264) {
            pCodecCtx->qmin = 10;
            pCodecCtx->qmax = 51;
            pCodecCtx->max_b_frames = 0;
            av_dict_set(&param, "preset", "ultrafast", 0);
        }

        if (avcodec_open2(pCodecCtx, pCodec, &param) < 0) {
            return false;
        }

        m_videoStream = avformat_new_stream(m_videoFormatContext, pCodec);
        if (m_videoStream == nullptr) {
            return false;
        }

        m_videoStream->time_base.num = 1;
        m_videoStream->time_base.den = m_framerate;
        m_videoStream->codec = pCodecCtx;
        pFrameYUV = av_frame_alloc();
        m_out_buffer = static_cast<uint8_t *>(av_malloc(static_cast<size_t>(avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height))));
        avpicture_fill((AVPicture *)pFrameYUV, m_out_buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);

    }

    if(m_sysAudioCodecID && m_micAudioCodecID) {
        m_bMix = true;
        bool initSccess = init_context_amix(m_channels,0,0,m_audio_bitrate);
        if(!initSccess) {
            printf("Can not init_context_amix\n");
            return 1;
        }
    }

    if(m_micAudioCodecID != 0) {
        printf("FLQQ,audio encoder initialize\n\n");
        pCodec_a = avcodec_find_encoder(m_micAudioCodecID);

        if (!pCodec_a) {
            return false;
        }

        m_pMicCodecContext = avcodec_alloc_context3(pCodec_a);
        m_pMicCodecContext->channels = m_channels;
        m_pMicCodecContext->channel_layout = static_cast<uint64_t>(m_channels_layout);
        if (m_pMicCodecContext->channel_layout == 0) {
            m_pMicCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
            m_pMicCodecContext->channels = av_get_channel_layout_nb_channels(m_pMicCodecContext->channel_layout);
        }

        m_pMicCodecContext->sample_rate = m_samplerate;
        m_pMicCodecContext->sample_fmt = pCodec_a->sample_fmts[0];
        m_pMicCodecContext->bit_rate = m_audio_bitrate;
        m_pMicCodecContext->time_base.num = 1;
        m_pMicCodecContext->time_base.den = m_pMicCodecContext->sample_rate;

        if(m_micAudioCodecID == AV_CODEC_ID_AAC) {
            m_pMicCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
        }

        if (m_videoFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
            m_pMicCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(m_pMicCodecContext, pCodec_a, nullptr) < 0) {
            return false;
        }

        if(!m_bMix) {
            m_micAudioStream = avformat_new_stream(m_videoFormatContext, pCodec_a);
            if (nullptr == m_micAudioStream)
                return false;
            m_micAudioStream->time_base.num = 1;
            m_micAudioStream->time_base.den = m_pMicCodecContext->sample_rate;//48000
            m_micAudioStream->codec = m_pMicCodecContext;
        }
        m_convertedMicSamples = nullptr;
        if (!(m_convertedMicSamples = static_cast<uint8_t**>(calloc(static_cast<size_t>(m_pMicCodecContext->channels), sizeof(**m_convertedMicSamples))))) {
            printf("Could not allocate converted input sample pointers\n");
            return false;
        }
        m_convertedMicSamples[0] = nullptr;
    }
    if(m_sysAudioCodecID != 0) {
        printf("FLQQ,audio encoder initialize\n\n");
        pCodec_aCard = avcodec_find_encoder(m_sysAudioCodecID);
        if (!pCodec_aCard) {
            return false;
        }
        m_pSysCodecContext = avcodec_alloc_context3(pCodec_aCard);
        m_pSysCodecContext->channels = m_channels_card;
        m_pSysCodecContext->channel_layout = static_cast<uint64_t>(m_channels_card_layout);

        if (m_pSysCodecContext->channel_layout == 0) {
            m_pSysCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
            m_pSysCodecContext->channels = av_get_channel_layout_nb_channels(m_pSysCodecContext->channel_layout);
        }

        m_pSysCodecContext->sample_rate = m_samplerate_card;
        m_pSysCodecContext->sample_fmt = pCodec_aCard->sample_fmts[0];
        m_pSysCodecContext->bit_rate = m_audio_bitrate_card;
        m_pSysCodecContext->time_base.num = 1;
        m_pSysCodecContext->time_base.den = m_pSysCodecContext->sample_rate;

        if(m_sysAudioCodecID == AV_CODEC_ID_AAC) {
            m_pSysCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
        }

        if (m_videoFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
            m_pSysCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(m_pSysCodecContext, pCodec_aCard, nullptr) < 0) {
            return false;
        }

        if(!m_bMix) {
            m_sysAudioStream = avformat_new_stream(m_videoFormatContext, pCodec_aCard);
            if (m_sysAudioStream == nullptr) {
                return false;
            }
            m_sysAudioStream->time_base.num = 1;
            m_sysAudioStream->time_base.den = m_pSysCodecContext->sample_rate;//48000
            m_sysAudioStream->codec = m_pSysCodecContext;
        }
        m_convertedSysSamples = nullptr;
        if (!(m_convertedSysSamples = static_cast<uint8_t**>(calloc(static_cast<size_t>(m_pSysCodecContext->channels), sizeof(**m_convertedSysSamples))))) {
            printf("Could not allocate converted input sample pointers\n");
            return false;
        }
        m_convertedSysSamples[0] = nullptr;
    }
    if (m_bMix) {
        if(init_filters()!=0) {
            return false;
        }
    }

    if (avio_open(&m_videoFormatContext->pb, m_path, AVIO_FLAG_READ_WRITE) < 0) {
        return false;
    }

    av_dump_format(m_videoFormatContext, 0, m_path, 1);
    avformat_write_header(m_videoFormatContext, nullptr);
    m_micAudioFrame = 0;
    m_sysAudioFrame = 0;
    m_nb_samples = 0;
    m_nLastAudioPresentationTime = 0;
    m_nLastAudioMixPresentationTime = 0;
    m_mixCount = 0;
    m_next_vid_time = 0;
    m_next_aud_time = 0;
    m_start_mix_time = -1;
    setIsWriteFrame(true);
    fflush(stdout);
    return true;
}

int AVOutputStream::writeVideoFrame(DateAdmin::waylandFrame &frame)
{
    if (0 == frame._time
            || nullptr == frame._frame
            || 0 == frame._index
            || 0 == frame._width
            || 0 == frame._height) {
        return -1;
    }

    AVFrame * pRgbFrame = av_frame_alloc();
    pRgbFrame->width = frame._width;
    pRgbFrame->height = frame._height;
    pRgbFrame->format = AV_PIX_FMT_RGB32;

    if (0 == av_frame_get_buffer(pRgbFrame,32)) {
        pRgbFrame->width  = frame._width;
        pRgbFrame->height = frame._height;
        pRgbFrame->crop_left   = static_cast<size_t>(m_left);
        pRgbFrame->crop_top    = static_cast<size_t>(m_top);
        pRgbFrame->crop_right  = static_cast<size_t>(m_right);
        pRgbFrame->crop_bottom = static_cast<size_t>(m_bottom);
        pRgbFrame->linesize[0] = frame._stride;
        pRgbFrame->data[0]     = frame._frame;
    }

    if (nullptr == m_pVideoSwsContext) {
        m_pVideoSwsContext = sws_getContext(m_width, m_height,
                                            AV_PIX_FMT_RGB32,
                                            pCodecCtx->width,
                                            pCodecCtx->height,
                                            AV_PIX_FMT_YUV420P,
                                            SWS_BICUBIC,
                                            nullptr,
                                            nullptr,
                                            nullptr);
    }
    if (av_frame_apply_cropping(pRgbFrame,AV_FRAME_CROP_UNALIGNED)<0) {
        AVERROR(ERANGE);
        return 2;
    }

    sws_scale(m_pVideoSwsContext, pRgbFrame->data, pRgbFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
    pFrameYUV->width  = pRgbFrame->width;
    pFrameYUV->height = pRgbFrame->height;
    pFrameYUV->format = AV_PIX_FMT_YUV420P;
    AVPacket packet;
    packet.data = nullptr;
    packet.size = 0;
    av_init_packet(&packet);
    int enc_got_frame = 0;
    avcodec_encode_video2(pCodecCtx, &packet, pFrameYUV, &enc_got_frame);

    if (1 == enc_got_frame) {
        packet.stream_index = m_videoStream->index;
        packet.pts = static_cast<int64_t>(m_videoStream->time_base.den) * frame._time/AV_TIME_BASE;
        int ret = writeFrame(m_videoFormatContext, &packet);
        if (ret < 0) {
            char tmpErrString[128] = {0};
            printf("Could not write video frame, error: %s\n", av_make_error_string(tmpErrString, AV_ERROR_MAX_STRING_SIZE, ret));
            av_packet_unref(&packet);
            return ret;
        }
    }
    av_free_packet(&packet);
    av_frame_free(&pRgbFrame);
    fflush(stdout);
    return 0;
}

int AVOutputStream::writeMicAudioFrame(AVStream *stream, AVFrame *inputFrame, int64_t lTimeStamp)
{
    if(nullptr == m_micAudioStream)
        return -1;

    const int frameSize = m_pMicCodecContext->frame_size;
    int ret;
    if(nullptr == m_pMicAudioSwrContext) {
        m_pMicAudioSwrContext = swr_alloc_set_opts(nullptr,
                                                   av_get_default_channel_layout(m_pMicCodecContext->channels),
                                                   m_pMicCodecContext->sample_fmt,
                                                   m_pMicCodecContext->sample_rate,
                                                   av_get_default_channel_layout(stream->codec->channels),
                                                   stream->codec->sample_fmt,
                                                   stream->codec->sample_rate,
                                                   0,
                                                   nullptr);
        assert(m_pMicCodecContext->sample_rate == stream->codec->sample_rate);
        swr_init(m_pMicAudioSwrContext);
        if (nullptr == m_micAudioFifo) {
            m_micAudioFifo = audioFifoAlloc(m_pMicCodecContext->sample_fmt, m_pMicCodecContext->channels, 20*inputFrame->nb_samples);
        }
        is_fifo_scardinit++;
    }

    if ((ret = av_samples_alloc(m_convertedMicSamples, nullptr, m_pMicCodecContext->channels, inputFrame->nb_samples, m_pMicCodecContext->sample_fmt, 0)) < 0) {
        printf("Could not allocate converted input samples\n");
        av_freep(&(*m_convertedMicSamples)[0]);
        free(*m_convertedMicSamples);
        return ret;
    }

    if ((ret = swr_convert(m_pMicAudioSwrContext,m_convertedMicSamples, inputFrame->nb_samples,const_cast<const uint8_t**>(inputFrame->extended_data),inputFrame->nb_samples)) < 0) {
        printf("Could not convert input samples\n");
        return ret;
    }
    AVRational rational;
    int audioSize = audioFifoSize(m_micAudioFifo);
    int64_t timeshift = static_cast<int64_t>(audioSize * AV_TIME_BASE) / static_cast<int64_t>(stream->codec->sample_rate);
    m_micAudioFrame += inputFrame->nb_samples;

    if ((ret = audioFifoRealloc(m_micAudioFifo, audioFifoSize(m_micAudioFifo) + inputFrame->nb_samples)) < 0) {
        printf("Could not reallocate FIFO\n");
        return ret;
    }

    if (audioWrite(m_micAudioFifo, reinterpret_cast<void **>(m_convertedMicSamples), inputFrame->nb_samples) < inputFrame->nb_samples) {
        printf("Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }

    int64_t timeinc = static_cast<int64_t>(m_pMicCodecContext->frame_size * AV_TIME_BASE /stream->codec->sample_rate);

    if (lTimeStamp - timeshift > m_nLastAudioPresentationTime ) {
        m_nLastAudioPresentationTime = lTimeStamp - timeshift;
    }

    while (audioFifoSize(m_micAudioFifo) >= frameSize)
    {
        AVFrame *outputFrame = av_frame_alloc();
        if (!outputFrame) {
            return AVERROR(ENOMEM);
        }

        const int frame_size = FFMIN(audioFifoSize(m_micAudioFifo), m_pMicCodecContext->frame_size);
        outputFrame->nb_samples = frame_size;
        outputFrame->channel_layout = m_pMicCodecContext->channel_layout;
        outputFrame->format = m_pMicCodecContext->sample_fmt;
        outputFrame->sample_rate = m_pMicCodecContext->sample_rate;
        outputFrame->pts = av_rescale_q(m_nLastAudioPresentationTime, rational, m_micAudioStream->time_base);

        if ((ret = av_frame_get_buffer(outputFrame, 0)) < 0) {
            printf("Could not allocate output frame samples\n");
            av_frame_free(&outputFrame);
            return ret;
        }

        if (audioRead(m_micAudioFifo, (void **)outputFrame->data, frame_size) < frame_size) {
            printf("Could not read data from FIFO\n");
            return AVERROR_EXIT;
        }

        AVPacket outputPacket;
        av_init_packet(&outputPacket);
        outputPacket.data = nullptr;
        outputPacket.size = 0;
        int enc_got_frame_a = 0;

        if ((ret = avcodec_encode_audio2(m_pMicCodecContext, &outputPacket, outputFrame, &enc_got_frame_a)) < 0) {
            printf("Could not encode frame\n");
            av_packet_unref(&outputPacket);
            return ret;
        }

        if (enc_got_frame_a) {
            outputPacket.stream_index = m_micAudioStream->index;
            printf("output_packet.stream_index1  audio_st =%d\n", outputPacket.stream_index);
            outputPacket.pts = av_rescale_q(m_nLastAudioPresentationTime, rational, m_micAudioStream->time_base);
            if ((ret = writeFrame(m_videoFormatContext, &outputPacket)) < 0) {
                char tmpErrString[128] = {0};
                printf("Could not write audio frame, error: %s\n", av_make_error_string(tmpErrString, AV_ERROR_MAX_STRING_SIZE, ret));
                av_packet_unref(&outputPacket);
                return ret;
            }
            av_packet_unref(&outputPacket);
        }
        m_nb_samples += outputFrame->nb_samples;
        m_nLastAudioPresentationTime += timeinc;
        av_frame_free(&outputFrame);
    }
    return 0;
}

int AVOutputStream::writeMicToMixAudioFrame(AVStream *stream, AVFrame *inputFrame, int64_t lTimeStamp)
{
    Q_UNUSED(lTimeStamp)
    int ret;
    if(nullptr == m_pMicAudioSwrContext) {
        m_pMicAudioSwrContext = swr_alloc_set_opts(nullptr,
                                                   av_get_default_channel_layout(m_pMicCodecContext->channels),
                                                   m_pMicCodecContext->sample_fmt,
                                                   m_pMicCodecContext->sample_rate,
                                                   av_get_default_channel_layout(stream->codec->channels),
                                                   stream->codec->sample_fmt,
                                                   stream->codec->sample_rate,
                                                   0,
                                                   nullptr);
        assert(m_pMicCodecContext->sample_rate == stream->codec->sample_rate);
        swr_init(m_pMicAudioSwrContext);
        if (nullptr == m_micAudioFifo) {
            m_micAudioFifo = audioFifoAlloc(m_pMicCodecContext->sample_fmt, m_pMicCodecContext->channels, 20*inputFrame->nb_samples);
        }
        is_fifo_scardinit++;
    }
    if ((ret = av_samples_alloc(m_convertedMicSamples, nullptr, m_pMicCodecContext->channels, inputFrame->nb_samples, m_pMicCodecContext->sample_fmt, 0)) < 0) {
        printf("Could not allocate converted input samples\n");
        av_freep(&(*m_convertedMicSamples)[0]);
        free(*m_convertedMicSamples);
        return ret;
    }
    if ((ret = swr_convert(m_pMicAudioSwrContext,m_convertedMicSamples, inputFrame->nb_samples,const_cast<const uint8_t**>(inputFrame->extended_data),inputFrame->nb_samples)) < 0) {
        printf("Could not convert input samples\n");
        return ret;
    }
    int fifoSpace = audioFifoSpace(m_micAudioFifo);
    int checkTime = 0;
    while(fifoSpace < inputFrame->nb_samples
          && isWriteFrame()
          && checkTime <= 1000)
    {
        fifoSpace = audioFifoSpace(m_micAudioFifo);
        usleep(10*1000);
        checkTime ++;
    }
    if (fifoSpace >= inputFrame->nb_samples) {
        if (audioWrite(m_micAudioFifo, (void **)m_convertedMicSamples, inputFrame->nb_samples) < inputFrame->nb_samples) {
            printf("Could not write data to FIFO\n");
            return AVERROR_EXIT;
        }
        printf("_fifo_spk write secceffull  m_fifo!\n");
    }
    return 0;
}
int AVOutputStream::write_filter_audio_frame(AVStream *&outst,AVCodecContext* &codecCtx_audio,AVFrame *&output_frame)
{
    int ret = 0;
    AVPacket output_packet;
    av_init_packet(&output_packet);
    output_packet.data = nullptr;
    output_packet.size = 0;
    int enc_got_frame_a = 0;
    if ((ret = avcodec_encode_audio2(codecCtx_audio, &output_packet, output_frame, &enc_got_frame_a)) < 0) {
        printf("Could not encode frame\n");
        av_packet_unref(&output_packet);
        return ret;
    }
    if (enc_got_frame_a) {
        output_packet.stream_index = outst->index;
        printf("output_packet.stream_index2  audio_st =%d\n", output_packet.stream_index);
#if 0
        AVRational r_framerate1 = { input_st->codec->sample_rate, 1 };// { 44100, 1};
        int64_t_t calc_pts = (double)m_nb_samples * (AV_TIME_BASE)*(1 / av_q2d(r_framerate1));
        output_packet.pts = av_rescale_q(calc_pts, time_base_q, audio_st->time_base);
#else

#endif
        if ((ret = writeFrame(m_videoFormatContext, &output_packet)) < 0) {
            char tmpErrString[128] = {0};
            printf("Could not write audio frame, error: %s\n", av_make_error_string(tmpErrString, AV_ERROR_MAX_STRING_SIZE, ret));
            av_packet_unref(&output_packet);
            return ret;
        }

        av_packet_unref(&output_packet);
    }
    av_frame_unref(output_frame);
    return ret;
}

void AVOutputStream::writeMixAudio()
{
    if (is_fifo_scardinit <2 || nullptr == m_sysAudioFifo || nullptr == m_micAudioFifo)
        return;
    AVFrame* pFrame_scard = av_frame_alloc();
    AVFrame* pFrame_temp = av_frame_alloc();
    int sysFifosize = audioFifoSize(m_sysAudioFifo);
    int micFifoSize = audioFifoSize(m_micAudioFifo);
    int minMicFrameSize = m_pMicCodecContext->frame_size;
    int minSysFrameSize = m_pSysCodecContext->frame_size;
    int ret;
    if (micFifoSize >= minMicFrameSize && sysFifosize >= minSysFrameSize) {
        tmpFifoFailed = 0;
        AVRational time_base_q;
        pFrame_scard->nb_samples = minSysFrameSize;
        pFrame_scard->channel_layout = m_pSysCodecContext->channel_layout;
        pFrame_scard->format = m_pSysCodecContext->sample_fmt;
        pFrame_scard->sample_rate = m_pSysCodecContext->sample_rate;
        av_frame_get_buffer(pFrame_scard, 0);
        pFrame_temp->nb_samples = minMicFrameSize;
        pFrame_temp->channel_layout = m_pMicCodecContext->channel_layout;
        pFrame_temp->format = m_pMicCodecContext->sample_fmt;
        pFrame_temp->sample_rate = m_pMicCodecContext->sample_rate;
        av_frame_get_buffer(pFrame_temp, 0);
        ret = audioRead(m_sysAudioFifo, reinterpret_cast<void **>(pFrame_scard->data), minSysFrameSize);
        ret = audioRead(m_micAudioFifo, reinterpret_cast<void **>(pFrame_temp->data), minMicFrameSize);
        int nFifoSamples = pFrame_scard->nb_samples;
        if (m_start_mix_time == -1) {
            printf("First Audio timestamp: %ld \n", av_gettime());
            m_start_mix_time = av_gettime();
        }
        int64_t lTimeStamp = av_gettime() - m_start_mix_time;
        int64_t timeshift = (int64_t)nFifoSamples * AV_TIME_BASE /(int64_t)(audio_amix_st->codec->sample_rate);
        if(lTimeStamp - timeshift > m_nLastAudioMixPresentationTime ) {
            m_nLastAudioMixPresentationTime = lTimeStamp - timeshift;
        }
        pFrame_scard->pts = av_rescale_q(m_nLastAudioMixPresentationTime, time_base_q, audio_amix_st->time_base);
        pFrame_temp->pts = pFrame_scard->pts;
        int64_t timeinc = (int64_t)pCodecCtx_amix->frame_size * AV_TIME_BASE /(int64_t)(audio_amix_st->codec->sample_rate);
        m_nLastAudioMixPresentationTime += timeinc;
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx1, pFrame_temp,0);
        if (ret < 0) {
            printf("Mixer: failed to call av_buffersrc_add_frame (speaker)\n");
            return;
        }
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx2, pFrame_scard,0);
        if (ret < 0) {
            printf("Mixer: failed to call av_buffersrc_add_frame (microphone)\n");
            return;
        }
        while (isWriteFrame())
        {
            AVFrame* pFrame_out = av_frame_alloc();
            ret = av_buffersink_get_frame(buffersink_ctx, pFrame_out);
            if (ret < 0) {
                printf("Mixer: failed to call av_buffersink_get_frame_flags ret : %d \n",ret);
                break;
            }
            fflush(stdout);
            if (pFrame_out->data[0] != nullptr) {
                AVPacket packet_out;
                av_init_packet(&packet_out);
                packet_out.data = nullptr;
                packet_out.size = 0;
                int got_packet_ptr;
                ret = avcodec_encode_audio2(pCodecCtx_amix, &packet_out, pFrame_out, &got_packet_ptr);
                if (ret < 0) {
                    printf("Mixer: failed to call avcodec_decode_audio4\n");
                    break;
                }
                if (got_packet_ptr) {
                    packet_out.stream_index = audio_amix_st->index;
                    packet_out.pts = m_mixCount * pCodecCtx_amix->frame_size;
                    packet_out.dts = packet_out.pts;
                    packet_out.duration = pCodecCtx_amix->frame_size;
                    packet_out.pts = av_rescale_q_rnd(packet_out.pts,
                                                      pCodecCtx_amix->time_base,
                                                      pCodecCtx_amix->time_base,
                                                      (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    packet_out.dts = packet_out.pts;
                    packet_out.duration = av_rescale_q_rnd(packet_out.duration,
                                                           pCodecCtx_amix->time_base,
                                                           audio_amix_st->time_base,
                                                           (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

                    m_mixCount++;
                    ret = writeFrame(m_videoFormatContext, &packet_out);
                    if (ret < 0) {
                        printf("Mixer: failed to call av_interleaved_write_frame\n");
                    }
                    printf("-------Mixer: write frame to file got_packet_ptr =%d\n",got_packet_ptr);
                }
                av_free_packet(&packet_out);
            }
            av_frame_free(&pFrame_out);
        }
    } else {
        tmpFifoFailed++;
        usleep(20*1000);
        if (tmpFifoFailed > 300) {
            usleep(30*1000);
            return;
        }
    }
    av_frame_free(&pFrame_scard);
    av_frame_free(&pFrame_temp);
}

int  AVOutputStream::writeSysAudioFrame(AVStream *stream, AVFrame *inputFrame, int64_t lTimeStamp)
{
    if(nullptr == m_sysAudioStream)
        return -1;
    const int output_frame_size = m_pSysCodecContext->frame_size;
    int ret;
    if(nullptr == m_pSysAudioSwrContext) {
        m_pSysAudioSwrContext = swr_alloc_set_opts(nullptr,
                                                   av_get_default_channel_layout(m_pSysCodecContext->channels),
                                                   m_pSysCodecContext->sample_fmt,
                                                   m_pSysCodecContext->sample_rate,
                                                   av_get_default_channel_layout(stream->codec->channels),
                                                   stream->codec->sample_fmt,
                                                   stream->codec->sample_rate,
                                                   0,
                                                   nullptr);

        assert(m_pSysCodecContext->sample_rate == stream->codec->sample_rate);
        swr_init(m_pSysAudioSwrContext);
        if(nullptr == m_sysAudioFifo) {
            m_sysAudioFifo = audioFifoAlloc(m_pSysCodecContext->sample_fmt, m_pSysCodecContext->channels, 20*inputFrame->nb_samples);
        }
        is_fifo_scardinit ++;
    }

    if ((ret = av_samples_alloc(m_convertedSysSamples, nullptr, m_pSysCodecContext->channels, inputFrame->nb_samples, m_pSysCodecContext->sample_fmt, 0)) < 0) {
        av_freep(&(*m_convertedSysSamples)[0]);
        free(*m_convertedSysSamples);
        return ret;
    }

    if ((ret = swr_convert(m_pSysAudioSwrContext,m_convertedSysSamples, inputFrame->nb_samples,const_cast<const uint8_t**>(inputFrame->extended_data), inputFrame->nb_samples)) < 0) {
        return ret;
    }
    AVRational rational = {1, AV_TIME_BASE };
    int audioSize = audioFifoSize(m_sysAudioFifo);
    int64_t timeshift = (int64_t)audioSize * AV_TIME_BASE /(int64_t)(stream->codec->sample_rate);
    m_sysAudioFrame += inputFrame->nb_samples;
    if ((ret = audioFifoRealloc(m_sysAudioFifo, audioFifoSize(m_sysAudioFifo) + inputFrame->nb_samples)) < 0) {
        printf("Could not reallocate FIFO\n");
        return ret;
    }

    if (audioWrite(m_sysAudioFifo, reinterpret_cast<void **>(m_convertedSysSamples), inputFrame->nb_samples) < inputFrame->nb_samples) {
        printf("Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }

    int64_t timeinc = static_cast<int64_t>(m_pSysCodecContext->frame_size * AV_TIME_BASE /stream->codec->sample_rate);
    if(lTimeStamp - timeshift > m_nLastAudioCardPresentationTime ) {
        m_nLastAudioCardPresentationTime = lTimeStamp - timeshift;
    }

    while (audioFifoSize(m_sysAudioFifo) >= output_frame_size)
    {
        AVFrame *output_frame = av_frame_alloc();
        if (!output_frame) {
            return AVERROR(ENOMEM);
        }

        const int frame_size = FFMIN(audioFifoSize(m_sysAudioFifo), m_pSysCodecContext->frame_size);
        output_frame->nb_samples = frame_size;
        output_frame->channel_layout = m_pSysCodecContext->channel_layout;
        output_frame->format = m_pSysCodecContext->sample_fmt;
        output_frame->sample_rate = m_pSysCodecContext->sample_rate;
        output_frame->pts = av_rescale_q(m_nLastAudioCardPresentationTime, rational, m_sysAudioStream->time_base);
        if ((ret = av_frame_get_buffer(output_frame, 0)) < 0) {
            printf("Could not allocate output frame samples\n");
            av_frame_free(&output_frame);
            return ret;
        }

        if (audioRead(m_sysAudioFifo, (void **)output_frame->data, frame_size) < frame_size) {
            printf("Could not read data from FIFO\n");
            return AVERROR_EXIT;
        }
        AVPacket outputPacket;
        av_init_packet(&outputPacket);
        outputPacket.data = nullptr;
        outputPacket.size = 0;
        int enc_got_frame_a = 0;

        if ((ret = avcodec_encode_audio2(m_pSysCodecContext, &outputPacket, output_frame, &enc_got_frame_a)) < 0) {
            printf("Could not encode frame\n");
            av_packet_unref(&outputPacket);
            return ret;
        }
        if (enc_got_frame_a) {
            outputPacket.stream_index = m_sysAudioStream->index;
            outputPacket.pts = av_rescale_q(m_nLastAudioCardPresentationTime, rational, m_sysAudioStream->time_base);
            if ((ret = writeFrame(m_videoFormatContext, &outputPacket)) < 0) {
                char tmpErrString[128] = {0};
                printf("Could not write audio frame, error: %s\n", av_make_error_string(tmpErrString, AV_ERROR_MAX_STRING_SIZE, ret));
                av_packet_unref(&outputPacket);
                return ret;
            }
            av_packet_unref(&outputPacket);
        }
        m_nb_samples += output_frame->nb_samples;
        m_nLastAudioCardPresentationTime += timeinc;
        av_frame_free(&output_frame);
    }
    return 0;
}

int AVOutputStream::writeSysToMixAudioFrame(AVStream *stream, AVFrame *inputFrame, int64_t lTimeStamp)
{
    Q_UNUSED(lTimeStamp)
    int ret;
    if (nullptr == m_pSysAudioSwrContext) {
        m_pSysAudioSwrContext = swr_alloc_set_opts(nullptr,
                                                   av_get_default_channel_layout(m_pSysCodecContext->channels),
                                                   m_pSysCodecContext->sample_fmt,
                                                   m_pSysCodecContext->sample_rate,
                                                   av_get_default_channel_layout(stream->codec->channels),
                                                   stream->codec->sample_fmt,
                                                   stream->codec->sample_rate,
                                                   0,
                                                   nullptr);
        assert(m_pSysCodecContext->sample_rate == stream->codec->sample_rate);
        swr_init(m_pSysAudioSwrContext);
        if(nullptr == m_sysAudioFifo) {
            m_sysAudioFifo = audioFifoAlloc(m_pSysCodecContext->sample_fmt, m_pSysCodecContext->channels, 20*inputFrame->nb_samples);
        }
        is_fifo_scardinit ++;
    }
    if ((ret = av_samples_alloc(m_convertedSysSamples, nullptr, m_pSysCodecContext->channels, inputFrame->nb_samples, m_pSysCodecContext->sample_fmt, 0)) < 0) {
        av_freep(&(*m_convertedSysSamples)[0]);
        free(*m_convertedSysSamples);
        return ret;
    }
    if ((ret = swr_convert(m_pSysAudioSwrContext,m_convertedSysSamples, inputFrame->nb_samples,const_cast<const uint8_t**>(inputFrame->extended_data), inputFrame->nb_samples)) < 0) {
        return ret;
    }
    int fifoSpace = audioFifoSpace(m_sysAudioFifo);
    int checkTime = 0;
    while(fifoSpace < inputFrame->nb_samples
          && isWriteFrame()
          && checkTime < 1000)
    {
        fifoSpace = audioFifoSpace(m_sysAudioFifo);
        usleep(10*1000);
        checkTime ++;
    }
    if (fifoSpace >= inputFrame->nb_samples) {
        if (audioWrite(m_sysAudioFifo, reinterpret_cast<void **>(m_convertedSysSamples), inputFrame->nb_samples) < inputFrame->nb_samples) {
            printf("Could not write data to FIFO\n");
            return AVERROR_EXIT;
        }
    }
    return 0;
}

void  AVOutputStream::close()
{
    QThread::msleep(500);
    if (nullptr != m_videoFormatContext
            || nullptr != m_videoStream
            || nullptr != m_micAudioStream
            || nullptr != m_sysAudioStream
            || nullptr != audio_amix_st) {
        writeTrailer(m_videoFormatContext);
    }

    if (m_videoStream) {
        avcodec_close(m_videoStream->codec);
        m_videoStream = nullptr;
    }

    if (m_micAudioStream) {
        avcodec_close(m_micAudioStream->codec);
        m_micAudioStream = nullptr;
    }

    if (audio_amix_st) {
        avcodec_close(audio_amix_st->codec);
        audio_amix_st = nullptr;
    }

    if (m_out_buffer) {
        av_free(m_out_buffer);
        m_out_buffer = nullptr;
    }
    if (m_convertedMicSamples) {
        av_freep(&m_convertedMicSamples[0]);
        m_convertedMicSamples = nullptr;
    }

    if (m_convertedSysSamples) {
        av_freep(&m_convertedSysSamples[0]);
        m_convertedSysSamples = nullptr;
    }

    is_fifo_scardinit = 0;

    if (m_videoFormatContext) {
        avio_close(m_videoFormatContext->pb);
    }
    avformat_free_context(m_videoFormatContext);
    m_videoFormatContext = nullptr;
    m_videoCodecID  = AV_CODEC_ID_NONE;
    m_micAudioCodecID = AV_CODEC_ID_NONE;
    m_sysAudioCodecID = AV_CODEC_ID_NONE;
    if (m_bMix) {
        av_frame_free(&mMic_frame);
        av_frame_free(&mSpeaker_frame);
        avfilter_graph_free(&filter_graph);
    }
}
void AVOutputStream::setIsOverWrite(bool isCOntinue)
{
    m_isOverWrite = isCOntinue;
}

int AVOutputStream::audioRead(AVAudioFifo *af, void **data, int nb_samples)
{
    QMutexLocker locker(&m_audioReadWriteMutex);
    return av_audio_fifo_read(af, data, nb_samples);
}

int AVOutputStream::audioWrite(AVAudioFifo *af, void **data, int nb_samples)
{
    QMutexLocker locker(&m_audioReadWriteMutex);
    return av_audio_fifo_write(af, data, nb_samples);
}

int AVOutputStream::writeFrame(AVFormatContext *s, AVPacket *pkt)
{
    QMutexLocker locker(&m_writeFrameMutex);
    return av_interleaved_write_frame(s, pkt);
}

int AVOutputStream::writeTrailer(AVFormatContext *s)
{
    QMutexLocker locker(&m_writeFrameMutex);
    return av_write_trailer(s);
}

int AVOutputStream::audioFifoSpace(AVAudioFifo *af)
{
    QMutexLocker locker(&m_audioReadWriteMutex);
    return av_audio_fifo_space(af);
}

int AVOutputStream::audioFifoSize(AVAudioFifo *af)
{
    QMutexLocker locker(&m_audioReadWriteMutex);
    return av_audio_fifo_size(af);
}

int AVOutputStream::audioFifoRealloc(AVAudioFifo *af, int nb_samples)
{
    QMutexLocker locker(&m_audioReadWriteMutex);
    return av_audio_fifo_realloc(af,nb_samples);
}

AVAudioFifo *AVOutputStream::audioFifoAlloc(AVSampleFormat sample_fmt, int channels, int nb_samples)
{
    QMutexLocker locker(&m_audioReadWriteMutex);
    return av_audio_fifo_alloc(sample_fmt,channels,nb_samples);
}

void AVOutputStream::audioFifoFree(AVAudioFifo *af)
{
    QMutexLocker locker(&m_audioReadWriteMutex);
    av_audio_fifo_free(af);
}

bool AVOutputStream::isWriteFrame()
{
    QMutexLocker locker(&m_isWriteFrameMutex);
    return m_isWriteFrame;
}

void AVOutputStream::setIsWriteFrame(bool isWriteFrame)
{
    QMutexLocker locker(&m_isWriteFrameMutex);
    m_isWriteFrame = isWriteFrame;
}
