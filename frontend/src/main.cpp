#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#include "widget.h"

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

    Widget widget;
    widget.show();

    return app.exec();
}
