#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QObject>
#include <QLabel>
#include <QThread>
#include <QImage>
#include <QMutex>
#include <QDebug>
#include <QCoreApplication>
#include <QTime>
// FFmpeg 头文件
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class VideoPlayer : public QThread
{
    Q_OBJECT

public:
    explicit VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer();

    bool loadVideo(const QString &filePath);
    void play();
    void pause();
    void stop();
    void setVideoLabel(QLabel *label); // 绑定 QLabel 显示视频

protected:
    void run() override; // 线程执行视频解码和显示

private:
    void cleanup(); // 释放资源
    void delay(int msec); // 延时函数

private:
    AVFormatContext *pFormatCtx = nullptr;
    AVCodecContext *pAVctx = nullptr;
    const AVCodec *pCodec = nullptr;
    AVPacket *pAVpkt = nullptr;
    AVFrame *pAVframe = nullptr;
    AVFrame *pAVframeRGB = nullptr;
    SwsContext *pSwsCtx = nullptr;
    unsigned char *buf = nullptr;
    int streamIndex = -1;

    QLabel *videoLabel = nullptr;
    QMutex mutex;
    bool isPlaying = false;
    bool isPaused = false;
};

#endif // VIDEOPLAYER_H
