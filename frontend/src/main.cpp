#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#include <QDir>
#include <iostream>
#include <string>
#include "widget.h"
#include "SeedData.h"
#include "HealthManager.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // 控制台代码页切到 UTF-8
    SetConsoleOutputCP(CP_UTF8);
#endif

    QApplication app(argc, argv);

    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), "qt", "_",
            QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // ---- 命令行参数处理 ----
    bool doSeed = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--seed" || arg == "-s") {
            doSeed = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "OmniHealth Profiler — 个人健康数字孪生系统\n\n"
                      << "用法: " << argv[0] << " [选项]\n\n"
                      << "选项:\n"
                      << "  --seed, -s     生成样例数据到数据库\n"
                      << "  --help, -h     显示帮助信息\n";
            return 0;
        }
    }

    // 如果指定 --seed，先生成样例数据
    if (doSeed) {
        auto seedMgr = health::createHealthManager(
            QDir(QCoreApplication::applicationDirPath())
                .filePath("omnihealth.db").toStdString());
        int n = seedSampleData(seedMgr.get());
        std::cout << "\n样例数据已生成（" << n << " 条记录），启动 GUI...\n" << std::endl;
    }

    Widget widget;
    widget.show();

    return app.exec();
}
