// Generate PNG icon assets from the project's SVG; no network or GUI is used.
#include <QCoreApplication>
#include <QSvgRenderer>
#include <QPainter>
#include <QImage>
#include <QDir>
#include <QFileInfo>
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(argc!=3) return 2;
    QSvgRenderer svg(QString::fromLocal8Bit(argv[1]));
    if(!svg.isValid()) return 3;
    QDir dir(QString::fromLocal8Bit(argv[2]));
    if(!dir.exists() && !dir.mkpath(".")) return 4;
    for(int size: {16,32,128,256,512}) for(int scale: {1,2}) {
        QImage image(size*scale,size*scale,QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
        svg.render(&painter); painter.end();
        auto name=QString("icon_%1x%1%2.png").arg(size).arg(scale==2?"@2x":"");
        if(!image.save(dir.filePath(name))) return 5;
    }
}
