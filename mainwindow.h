#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QLabel>  // 添加 QLabel 头文件
#include "videoplayer.h"
#include "audiorecorder.h"
#include "audioplayer.h"

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

    void on_recordButton_clicked();

    void on_audioButton_clicked();

private:
    Ui::MainWindow *ui;
    VideoPlayer * videoplayer;
    AudioPlayer * audioplayer;
    AudioRecorder * recorder;
};

#endif // MAINWINDOW_H
