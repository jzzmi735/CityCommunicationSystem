/**
 * @file    main.cpp
 * @brief   城市通信网络设计系统 —— 程序入口。
 *
 * 对应《开发者 C 工作说明》第 25 节：
 * 本文件只负责启动界面，**不得**写入任何具体算法的测试代码。
 * 各模块的测试一律放在 tests/ 目录下，由独立的测试目标承载。
 */

#include <QApplication>

#include "gui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QApplication::setApplicationName(QStringLiteral("城市通信网络设计系统"));
    QApplication::setOrganizationName(QStringLiteral("CityCommunicationSystem"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    MainWindow window;
    window.show();

    return app.exec();
}
