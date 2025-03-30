#include "audioplayer.h"

AudioPlayer::AudioPlayer(QObject *parent) : QThread(parent) {
    avdevice_register_all();
    avformat_network_init(); // 初始化 FFmpeg 网络支持
}

AudioPlayer::~AudioPlayer() {
    stopPlaying();
}

void AudioPlayer::startPlaying(const QString &filePath) {
    if (!isRunning()) {
        playing = true;
        audioFile = filePath;
        start();
    }
}

void AudioPlayer::stopPlaying() {
    playing = false;
    requestInterruption();
    wait();
}

void AudioPlayer::run() {
    qDebug() << "音频播放线程启动";

    // **1. 打开音频文件**
    if (avformat_open_input(&fmtCtx, audioFile.toStdString().c_str(), nullptr, nullptr) < 0) {
        qDebug() << "无法打开音频文件：" << audioFile;
        return;
    }

    // **2. 查找音频流**
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        qDebug() << "无法获取音频流信息";
        cleanup();
        return;
    }

    int streamIndex = -1;
    for (unsigned i = 0; i < fmtCtx->nb_streams; i++) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            streamIndex = i;
            break;
        }
    }
    if (streamIndex == -1) {
        qDebug() << "未找到音频流";
        cleanup();
        return;
    }
    qDebug() << "找到音频流";
    // **3. 获取解码器**
    AVCodecParameters *codecParams = fmtCtx->streams[streamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) {
        qDebug() << "找不到合适的解码器";
        cleanup();
        return;
    }

    codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecParams);
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        qDebug() << "无法打开解码器";
        cleanup();
        return;
    }
    qDebug() << "打开解码器";
    // **4. 初始化音频重采样 (FFmpeg -> Qt)**

    swrCtx = swr_alloc();

    if (!swrCtx) {
        qDebug() << "Failed to allocate SwrContext";
        return;
    }

    // 2. 定义输入和输出通道布局（FFmpeg 5.1+ 需要 AVChannelLayout）
    AVChannelLayout in_ch_layout, out_ch_layout;
    av_channel_layout_default(&in_ch_layout, 6);  // 5.1 声道（6个通道）
    av_channel_layout_default(&out_ch_layout, 2); // 立体声（2个通道）

    // 3. 设置输入通道布局、采样率、格式
    av_opt_set_chlayout(swrCtx, "in_chlayout", &in_ch_layout, 0);
    av_opt_set_int(swrCtx, "in_sample_rate", 48000, 0);
    av_opt_set_sample_fmt(swrCtx, "in_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);

    // 4. 设置输出通道布局、采样率、格式（匹配 Qt）
    av_opt_set_chlayout(swrCtx, "out_chlayout", &out_ch_layout, 0);
    av_opt_set_int(swrCtx, "out_sample_rate", 44100, 0);
    av_opt_set_sample_fmt(swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    // 5. 初始化重采样
    if (swr_init(swrCtx) < 0) {
        qDebug() << "Failed to initialize SwrContext";
    } else {
        qDebug() << "SwrContext initialized successfully.";
    }

    // 6. 释放通道布局，避免内存泄漏
    // av_channel_layout_uninit(&in_ch_layout);
    // av_channel_layout_uninit(&out_ch_layout);

    qDebug() << "重新采样结束";


    // **5. 设置 Qt 音频格式**
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    // 获取默认音频输出设备
    QAudioDevice audioDevice = QMediaDevices::defaultAudioOutput();

    // 创建 QAudioSink
    audioSink = new QAudioSink(audioDevice, format);

    // 启动音频播放
    QIODevice *device = audioSink->start();  // 这里的 device 是音频输出流
    qDebug() << "设置音频格式结束";

    // **6. 解码 & 播放**
    AVPacket packet;
    AVFrame *frame = av_frame_alloc();

    while (playing && !isInterruptionRequested()) {
        qDebug() << "读取音频帧";
        if (av_read_frame(fmtCtx, &packet) >= 0) {
            if (packet.stream_index == streamIndex) {
                qDebug() << "解码音频包";
                if (avcodec_send_packet(codecCtx, &packet) == 0) {
                    while (avcodec_receive_frame(codecCtx, frame) == 0) {
                        qDebug() << "音频帧解码成功，样本数: " << frame->nb_samples;

                        // 1. 计算目标样本数
                        int maxDstNbSamples = av_rescale_rnd(frame->nb_samples, 44100, 48000, AV_ROUND_UP);
                        int outputBufferSize = av_samples_get_buffer_size(nullptr, 2, maxDstNbSamples, AV_SAMPLE_FMT_S16, 1);


                        // 2. 分配输出缓冲区
                        uint8_t *outputBuffer = (uint8_t *)av_malloc(outputBufferSize);
                        if (!outputBuffer) {
                            qDebug() << "outputBuffer 分配失败";
                            continue;
                        }
                        // 创建输出缓冲区数组
                        uint8_t *outBuf[2] = { outputBuffer, nullptr };
                        const uint8_t **inBuf = (const uint8_t **)frame->data;
                        if (!outputBuffer) {
                            qDebug() << "outputBuffer 分配失败";
                            continue;
                        }
                        qDebug() << "outputBuffer 分配成功";
                        // // 3. 执行重采样    这个地方死掉了
                       // 使用实际的帧大小进行重采样
                        int convertedSamples = swr_convert(swrCtx, 
                            outBuf,                    // 输出缓冲区
                            maxDstNbSamples,          // 输出采样数
                            inBuf,                     // 输入缓冲区
                            frame->nb_samples          // 输入采样数
                        );
                        qDebug() << "重新采样成功";
                        if (convertedSamples < 0) {
                            qDebug() << "swr_convert 失败，错误代码：" << convertedSamples;
                            av_free(outputBuffer);
                            continue;
                        }
                        qDebug() << "重采样 成功";
                        int bytesWritten =device->write((char *)frame->data[0], frame->linesize[0]);
                        // 4. 写入音频输出设备
                        //int bytesWritten = device->write((char *)outputBuffer, outputBufferSize);
                        if (bytesWritten < 0) {
                            qDebug() << "device->write 失败，返回值：" << bytesWritten;
                        }

                        // 5. 释放缓冲区
                        av_free(outputBuffer);
                    }
                }
            }
            av_packet_unref(&packet);
        } else {
            qDebug() << "读取音频帧失败，可能是文件结束";
            break;
        }
    }


    cleanup();
    qDebug() << "音频播放线程结束";
}

void AudioPlayer::cleanup() {
    if (audioSink) {
        audioSink->stop();
        delete audioSink;
        audioSink = nullptr;
    }
    if (codecCtx) {
        avcodec_free_context(&codecCtx);
    }
    if (fmtCtx) {
        avformat_close_input(&fmtCtx);
        fmtCtx = nullptr;
    }
    if (swrCtx) {
        swr_free(&swrCtx);
    }
}





