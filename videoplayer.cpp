#include "videoplayer.h"

VideoPlayer::VideoPlayer(QObject *parent) : QThread(parent)
{
    avformat_network_init(); // 初始化 FFmpeg
}

VideoPlayer::~VideoPlayer()
{
    stop(); // 确保释放资源
}

bool VideoPlayer::loadVideo(const QString &filePath)
{
    cleanup(); // 清理旧资源

    pFormatCtx = avformat_alloc_context();
    if (avformat_open_input(&pFormatCtx, filePath.toUtf8().constData(), nullptr, nullptr) != 0)
    {
        qDebug("avformat_open_input failed.");
        cleanup();
        return false;
    }

    if (avformat_find_stream_info(pFormatCtx, nullptr) < 0)
    {
        qDebug("avformat_find_stream_info failed.");
        cleanup();
        return false;
    }

    // 查找视频流
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            streamIndex = i;
            break;
        }
    }

    if (streamIndex == -1)
    {
        qDebug("No video stream found.");
        cleanup();
        return false;
    }

    // 初始化解码器
    pAVctx = avcodec_alloc_context3(nullptr);
    avcodec_parameters_to_context(pAVctx, pFormatCtx->streams[streamIndex]->codecpar);
    pCodec = avcodec_find_decoder(pAVctx->codec_id);
    if (!pCodec || avcodec_open2(pAVctx, pCodec, nullptr) < 0)
    {
        qDebug("Failed to open codec.");
        cleanup();
        return false;
    }

    pAVpkt = av_packet_alloc();
    pAVframe = av_frame_alloc();
    pAVframeRGB = av_frame_alloc();
    buf = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB32, pAVctx->width, pAVctx->height, 1));
    av_image_fill_arrays(pAVframeRGB->data, pAVframeRGB->linesize, buf, AV_PIX_FMT_RGB32, pAVctx->width, pAVctx->height, 1);

    pSwsCtx = sws_getContext(
        pAVctx->width, pAVctx->height, pAVctx->pix_fmt,
        pAVctx->width, pAVctx->height, AV_PIX_FMT_RGB32,
        SWS_BICUBIC, nullptr, nullptr, nullptr
        );

    return true;
}

void VideoPlayer::play()
{
    if (!isPlaying)
    {
        isPlaying = true;
        isPaused = false;
        start(); // 启动线程播放
    }
    else
    {
        isPaused = false;
    }
}

void VideoPlayer::pause()
{
    isPaused = true;
}

void VideoPlayer::stop()
{
    isPlaying = false;
    wait(); // 等待线程结束
    cleanup();
}

void VideoPlayer::setVideoLabel(QLabel *label)
{
    videoLabel = label;
}

void VideoPlayer::run()
{
    while (isPlaying)
    {
        if (isPaused)
        {
            delay(100);
            continue;
        }

        if (av_read_frame(pFormatCtx, pAVpkt) >= 0)
        {
            if (pAVpkt->stream_index == streamIndex)
            {
                if (avcodec_send_packet(pAVctx, pAVpkt) == 0)
                {
                    if (avcodec_receive_frame(pAVctx, pAVframe) == 0)
                    {
                        sws_scale(
                            pSwsCtx,
                            (const uint8_t *const *)pAVframe->data, pAVframe->linesize,
                            0, pAVctx->height,
                            pAVframeRGB->data, pAVframeRGB->linesize
                            );

                        QImage img((uchar *)pAVframeRGB->data[0], pAVctx->width, pAVctx->height, QImage::Format_RGB32);
                        if (videoLabel)
                        {
                            QMetaObject::invokeMethod(videoLabel, "setPixmap", Qt::QueuedConnection, Q_ARG(QPixmap, QPixmap::fromImage(img)));
                        }

                        delay(40); // 控制帧率
                    }
                }
            }
            av_packet_unref(pAVpkt);
        }
        else
        {
            break;
        }
    }
}

void VideoPlayer::cleanup()
{
    sws_freeContext(pSwsCtx);
    av_frame_free(&pAVframeRGB);
    av_frame_free(&pAVframe);
    av_packet_free(&pAVpkt);
    avcodec_free_context(&pAVctx);
    avformat_close_input(&pFormatCtx);
    av_free(buf);

    pSwsCtx = nullptr;
    pAVframeRGB = nullptr;
    pAVframe = nullptr;
    pAVpkt = nullptr;
    pAVctx = nullptr;
    pFormatCtx = nullptr;
    buf = nullptr;
}

void VideoPlayer::delay(int msec)
{
    QTime dieTime = QTime::currentTime().addMSecs(msec);
    while (QTime::currentTime() < dieTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}
