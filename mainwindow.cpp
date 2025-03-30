// 包含必要的Qt头文件
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

// 引入FFmpeg相关头文件
extern "C"
{
#include <libavcodec/avcodec.h>      // 编解码器
#include <libavformat/avformat.h>     // 封装格式处理
#include <libswscale/swscale.h>       // 图像转换
#include <libavdevice/avdevice.h>     // 设备相关
#include <libavformat/version.h>      // 版本信息
#include <libavutil/time.h>           // 时间工具
#include <libavutil/mathematics.h>     // 数学运算
#include <libavutil/imgutils.h>       // 图像工具
}

// 主窗口构造函数
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),recorder(new AudioRecorder(this))
{
    ui->setupUi(this);

    videoplayer = new VideoPlayer(this);
    audioplayer = new AudioPlayer(this);
    videoplayer->setVideoLabel(ui->video_window);

}
MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_open_button_clicked()
{

    QString filePath = QFileDialog::getOpenFileName(this, "选择视频文件", "", "Video Files (*.mp4 *.avi)");
    if (!filePath.isEmpty() && videoplayer->loadVideo(filePath))
    {
        videoplayer->play();
    }
}

void MainWindow::on_recordButton_clicked()
{
    if (!recorder->isRunning()) {
        recorder->startRecording();
        ui->recordButton->setText("停止录音");
    } else {
        recorder->stopRecording();
        ui->recordButton->setText("开始录音");
    }

}


void MainWindow::on_audioButton_clicked()
{

    if (!audioplayer->isRunning()) {
        QString filePath = QFileDialog::getOpenFileName(this, "选择视频文件", "", "Video Files (*.mp4 *.wav)");
        audioplayer->startPlaying(filePath);
        ui->audioButton->setText("停止播放音频");
    } else {

        audioplayer->stopPlaying();
        ui->audioButton->setText("开始播放音频");

    }
}

