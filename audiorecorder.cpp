#include "audiorecorder.h"

// 设备名称（Windows: dshow, Linux: alsa, macOS: avfoundation）
#ifdef _WIN32
#define FMT_NAME "dshow"
#define DEVICE_NAME "audio=麦克风阵列 (Realtek(R) Audio)"
#elif __linux__
#define FMT_NAME "alsa"
#define DEVICE_NAME "default"
#elif __APPLE__
#define FMT_NAME "avfoundation"
#define DEVICE_NAME ":0"
#endif

#define WAV_HEADER_SIZE 44

AudioRecorder::AudioRecorder(QObject *parent) : QThread(parent) {
    avdevice_register_all();
}

AudioRecorder::~AudioRecorder() {
    stopRecording();
}

void AudioRecorder::startRecording() {
    if (!isRunning()) {
        recording = true;
        start();
    }
}

void AudioRecorder::stopRecording() {
    recording = false;
    requestInterruption();
    wait();
}

void AudioRecorder::run() {
    qDebug() << "录音线程启动";

    const AVInputFormat *fmt = av_find_input_format(FMT_NAME);
    if (!fmt) {
        qDebug() << "无法找到输入格式";
        return;
    }

    int ret = avformat_open_input(&ctx, DEVICE_NAME, fmt, nullptr);
    if (ret < 0) {
        char errbuff[1024];
        av_strerror(ret, errbuff, sizeof(errbuff));
        qDebug() << "打开设备失败：" << errbuff;
        return;
    }
    QString savePath = "D:/qt5/untitled1/";  // 目标文件夹
    QString fileName = savePath + "output_" + QDateTime::currentDateTime().toString("yyyyMMddhhmmss") + ".wav";

    outputFile.setFileName(fileName);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        avformat_close_input(&ctx);
        qDebug() << "无法打开文件：" << fileName;
        return;
    }

    // 预留 WAV 头部
    if (outputFile.write(QByteArray(WAV_HEADER_SIZE, 0)) != WAV_HEADER_SIZE) {
        qDebug() << "写入 WAV 头部失败";
        outputFile.close();
        avformat_close_input(&ctx);
        return;
    }

    qDebug() << "开始录音，保存至：" << fileName;

    AVPacket pkt;
    while (recording && !isInterruptionRequested()) {
        ret = av_read_frame(ctx, &pkt);
        if (ret == 0) {
            qint64 written = outputFile.write((char *)pkt.data, pkt.size);
            if (written != pkt.size) {
                qDebug() << "数据写入失败，期望写入：" << pkt.size << " 实际写入：" << written;
                break;
            }
        } else {
            qDebug() << "读取音频数据失败";
            break;
        }
    }

    // 确保文件大小更新
    outputFile.flush();

    // 写入 WAV 头部
    writeWavHeader(outputFile, 44100, 2, 16);
    outputFile.close();
    avformat_close_input(&ctx);

    qDebug() << "录音线程结束";
}

void AudioRecorder::writeWavHeader(QFile &file, int sampleRate, int channels, int bitsPerSample) {
    file.seek(0);
    char header[WAV_HEADER_SIZE] = {0};

    memcpy(header, "RIFF", 4);
    int fileSize = file.size() - 8;
    memcpy(header + 4, &fileSize, 4);
    memcpy(header + 8, "WAVEfmt ", 8);

    int fmtSize = 16;
    memcpy(header + 16, &fmtSize, 4);
    short audioFormat = 1;
    memcpy(header + 20, &audioFormat, 2);
    memcpy(header + 22, &channels, 2);
    memcpy(header + 24, &sampleRate, 4);

    int byteRate = sampleRate * channels * bitsPerSample / 8;
    memcpy(header + 28, &byteRate, 4);
    short blockAlign = channels * bitsPerSample / 8;
    memcpy(header + 32, &blockAlign, 2);
    memcpy(header + 34, &bitsPerSample, 2);
    memcpy(header + 36, "data", 4);

    int dataSize = file.size() - WAV_HEADER_SIZE;
    memcpy(header + 40, &dataSize, 4);

    file.write(header, WAV_HEADER_SIZE);
}
