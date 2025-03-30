#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QThread>
#include <QFile>
#include <QAudioSink>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QIODevice>
#include <QDebug>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavdevice/avdevice.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

class AudioPlayer : public QThread {
    Q_OBJECT

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer();

    void startPlaying(const QString &filePath);
    void stopPlaying();

protected:
    void run() override;

private:
    void initFFmpeg();
    void cleanup();

    bool playing = false;
    QString audioFile;
    AVFormatContext *fmtCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    SwrContext *swrCtx = nullptr;
    QAudioSink *audioSink = nullptr;
    QIODevice *audioDevice = nullptr;
};

#endif // AUDIOPLAYER_H
