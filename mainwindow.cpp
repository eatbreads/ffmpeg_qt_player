#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QTime>
#include <QIcon>
#include <QPixmap>
#include <QFileDialog>
#include <QMessageBox>
#include <QSlider>
#include <QComboBox>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavformat/version.h>
#include <libavutil/time.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);


    qDebug("-----{version}----");
    qDebug("%d", avcodec_version());
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 延时函数
void delay(int msec)
{
    QTime dieTime = QTime::currentTime().addMSecs(msec);
    while (QTime::currentTime() < dieTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

void MainWindow::on_open_button_clicked()
{

    unsigned char* buf;
    int isVideo = -1;
    int ret;
    unsigned int i, streamIndex = 0;
    const AVCodec *pCodec;
    AVPacket *pAVpkt;
    AVCodecContext *pAVctx;
    AVFrame *pAVframe, *pAVframeRGB;
    AVFormatContext* pFormatCtx;
    struct SwsContext* pSwsCtx;

    avformat_network_init();
    //ffmpeg4.0之后了av移除_register_all();等函数，无需再进行初始化

    //char videoPath[] ="rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4";
    //视频流暂时无法播放，后续优化
    // 创建AVFormatContext

    QString filePath = QFileDialog::getOpenFileName(this, "选择视频文件", QCoreApplication::applicationDirPath(), "Video Files (*.mov *.mp4 *.avi)");
    if (filePath.isEmpty()) {
        return;
    }
    pFormatCtx = avformat_alloc_context();

    // 初始化pFormatCtx
    if (avformat_open_input(&pFormatCtx, filePath.toUtf8().constData(), 0, 0) != 0)
    {
        qDebug("avformat_open_input err.");
        avformat_free_context(pFormatCtx); // 释放资源
        return;
    }

    // 获取音视频流数据信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        avformat_close_input(&pFormatCtx);
        qDebug("avformat_find_stream_info err.");
        return;
    }

    // 找到视频流的索引
    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            streamIndex = i;
            isVideo = 0;
            break;
        }
    }

    // 没有视频流就退出
    if (isVideo == -1)
    {
        avformat_close_input(&pFormatCtx);
        qDebug("nb_streams err.");
        return;
    }

    // 获取视频流编码
    pAVctx = avcodec_alloc_context3(NULL);

    // 查找解码器
    avcodec_parameters_to_context(pAVctx, pFormatCtx->streams[streamIndex]->codecpar);
    //ffmpeg新版中不支持原来的codec，故上文替换为codecper
    pCodec = avcodec_find_decoder(pAVctx->codec_id);
    if (pCodec == NULL)
    {
        avcodec_free_context(&pAVctx); // 释放资源
        avformat_close_input(&pFormatCtx);
        qDebug("avcodec_find_decoder err.");
        return;
    }

    // 初始化pAVctx
    if (avcodec_open2(pAVctx, pCodec, NULL) < 0)
    {
        avcodec_free_context(&pAVctx); // 释放资源
        avformat_close_input(&pFormatCtx);
        qDebug("avcodec_open2 err.");
        return;
    }

    // 初始化pAVpkt
    pAVpkt = av_packet_alloc();
    if (!pAVpkt)
    {
        avcodec_free_context(&pAVctx); // 释放资源
        avformat_close_input(&pFormatCtx);
        qDebug("av_packet_alloc err.");
        return;
    }

    // 初始化数据帧空间
    pAVframe = av_frame_alloc();
    pAVframeRGB = av_frame_alloc();
    if (!pAVframe || !pAVframeRGB)
    {
        av_packet_free(&pAVpkt); // 释放资源
        avcodec_free_context(&pAVctx);
        avformat_close_input(&pFormatCtx);
        qDebug("av_frame_alloc err.");
        return;
    }

    // 创建图像数据存储buf
    buf = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB32, pAVctx->width, pAVctx->height, 1));
    av_image_fill_arrays(pAVframeRGB->data, pAVframeRGB->linesize, buf, AV_PIX_FMT_RGB32, pAVctx->width, pAVctx->height, 1);

    // 根据视频宽高重新调整窗口大小 视频宽高 pAVctx->width, pAVctx->height
    ui->video_window->setGeometry(0, 0, pAVctx->width, pAVctx->height);
    ui->video_window->setScaledContents(true);  // 设置 QLabel 内容缩放

    // 初始化pSwsCtx
    pSwsCtx = sws_getContext(pAVctx->width, pAVctx->height, pAVctx->pix_fmt, pAVctx->width, pAVctx->height, AV_PIX_FMT_RGB32, SWS_BICUBIC, NULL, NULL, NULL);

    // 循环读取视频数据
    int mVideoPlaySta = 1;
    while (true)
    {
        if (mVideoPlaySta == 1) // 正在播放
        {
            if (av_read_frame(pFormatCtx, pAVpkt) >= 0) // 读取一帧未解码的数据
            {
                // 如果是视频数据
                if (pAVpkt->stream_index == (int)streamIndex)
                {
                    // 解码一帧视频数据 旧版解码在新版中不适用，故使用
                    //avcodec_send_packet，avcodec_receive_frame代替，希望大家能够熟悉使用
                    ret = avcodec_send_packet(pAVctx, pAVpkt);
                    if (ret < 0)
                    {
                        qDebug("Decode Error: avcodec_send_packet");
                        av_packet_unref(pAVpkt);
                        continue;
                    }

                    ret = avcodec_receive_frame(pAVctx, pAVframe);
                    if (ret == 0)
                    {
                        sws_scale(pSwsCtx, (const unsigned char* const*)pAVframe->data, pAVframe->linesize, 0, pAVctx->height, pAVframeRGB->data, pAVframeRGB->linesize);
                        QImage img((uchar*)pAVframeRGB->data[0], pAVctx->width, pAVctx->height, QImage::Format_RGB32);
                        ui->video_window->setPixmap(QPixmap::fromImage(img));
                        delay(1); // 添加延时
                    }
                    else if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
                    {
                        qDebug("Decode Error: avcodec_receive_frame");
                    }
                }
                av_packet_unref(pAVpkt);
            }
            else
            {
                break;
            }
        }
        else // 暂停
        {
            delay(300);
        }
    }

    // 释放资源
    sws_freeContext(pSwsCtx);
    av_frame_free(&pAVframeRGB);
    av_frame_free(&pAVframe);
    av_packet_free(&pAVpkt);
    avcodec_free_context(&pAVctx);
    avformat_close_input(&pFormatCtx);
    qDebug() << "play finish!";
}
