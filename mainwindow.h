#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QLabel>  // 添加 QLabel 头文件
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
}

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_open_button_clicked();

private:
    Ui::MainWindow *ui;


    // FFmpeg 相关变量
    AVFormatContext *pFormatCtx = nullptr;
    AVCodecContext *pAVctx = nullptr;
    const AVCodec *pCodec = nullptr;
    AVPacket *pAVpkt = nullptr;
    AVFrame *pAVframe = nullptr;
    AVFrame *pAVframeRGB = nullptr;
    SwsContext *pSwsCtx = nullptr;
    unsigned char *buf = nullptr;
    int streamIndex = -1;
    int isVideo = -1;
};

#endif // MAINWINDOW_H
