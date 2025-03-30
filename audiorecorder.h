#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <QThread>
#include <QFile>
#include <QDebug>
#include <QDateTime>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

class AudioRecorder : public QThread
{
    Q_OBJECT

public:
    explicit AudioRecorder(QObject *parent = nullptr);
    ~AudioRecorder();

    void startRecording();
    void stopRecording();

protected:
    void run() override;

private:
    void writeWavHeader(QFile &file, int sampleRate, int channels, int bitsPerSample);

    AVFormatContext *ctx = nullptr;
    QFile outputFile;
    bool recording = false;
};

#endif // AUDIORECORDER_H
