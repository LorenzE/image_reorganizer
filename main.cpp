#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QCommandLineParser>

#include "exif.h"
#include <stdio.h>

QFileInfoList scanDir(const QString& scanDirPath,
                      const QStringList& fileEndings)
{
    qDebug() << "Scanning: " << scanDirPath;

    QDir dir(scanDirPath);
    dir.setNameFilters(fileEndings);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);

    QFileInfoList list;
    list << dir.entryInfoList();

    dir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    QStringList dirList = dir.entryList();
    QString newPath;

    for (int i = 0; i < dirList.size(); ++i) {
        newPath = QString("%1/%2").arg(dir.absolutePath()).arg(dirList.at(i));
        list << scanDir(newPath,
                        fileEndings);
    }

    return list;
}

void copy(const QFileInfoList& list,
          const QString& targetDirPath,
          int removeAfterCopy = 0)
{
    int counterCopy = 0;
    int counterRemove = 0;

    QDateTime dateTime;
    QString month;
    QString newFolder;
    QByteArray data;

    for(int i = 0; i < list.size(); ++i) {
        QFile file(list.at(i).filePath());
        if (file.open(QIODevice::ReadOnly)){
            data = file.readAll();
            easyexif::EXIFInfo info;
            if (int code = info.parseFrom((unsigned char *)data.data(), data.size())){
                qDebug() << "Error parsing EXIF: code " << code;
            }
            dateTime = QDateTime::fromString(QString::fromStdString(info.DateTimeOriginal), "yyyy:MM:dd hh:mm:ss");
            file.close();
        }

        if(dateTime.isValid() && !dateTime.isNull()) {
            //Option 1 Folder names YYYY/MM_MMMM
            month = dateTime.toString("MM")+"_"+dateTime.toString("MMMM");
            newFolder = targetDirPath + QString("/%1/%2/").arg(dateTime.date().year()).arg(month);

            //Option 2 Folder names YYYY-MM
            //newFolder = targetDirPath + QString("/%1-%2/").arg(dateTime.date().year()).arg(dateTime.toString("MM"));
        } else {
            newFolder = targetDirPath + QString("/unknown/");
        }

        if(!QDir(newFolder).exists()) {
            QDir().mkpath(newFolder);
        }

        if(QFile::copy(list.at(i).filePath(), newFolder+list.at(i).fileName())) {
            counterCopy++;
        }

        if(QFile::exists(list.at(i).filePath()) &&
           QFile::exists(newFolder+list.at(i).fileName()) &&
           removeAfterCopy == 1) {
            if(QFile::remove(list.at(i).filePath())) {
                counterRemove++;
            }
        }

        if(i%100==0) {
            qDebug()<<"Processed"<<i<<"photos. Still to go:"<<list.size()-i;
        }
    }

    qDebug() << "Copied" << counterCopy << "image files.";
    qDebug() << "Removed" << counterRemove << "image files.";
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Image Reorganizer: Reorganizes image files based on their creation date. Files with unknown creation dates are copied in the folder 'Unknown'.");
    parser.addHelpOption();

    QCommandLineOption scanDirPath({"s", "scan"}, "The <dir> which is recursivley scanned for image files.", "dir", "F:/CloudStation/Bilder/Handycam");
    QCommandLineOption targetDirPath({"t", "target"}, "The <dir> where the new files should be copied to.", "dir", "F:/Handy_Backup");
    QCommandLineOption removeAfterCopy({"r", "remove"}, "Set <option> to 0 to keep files and 1 to remove them after copying. Default is 0.", "option", "0");
    QCommandLineOption fileEndings({"f", "file"}, "Specify the file <endings> to search for. Must be seperated by a ','. Default is set to 3gp,mov,mp4,png,jpeg,jpg.", "endings", "3gp,mov,mp4,png,jpeg,jpg");

    parser.addOption(scanDirPath);
    parser.addOption(targetDirPath);
    parser.addOption(removeAfterCopy);
    parser.addOption(fileEndings);

    parser.process(a);

    if(!QDir().exists(parser.value(scanDirPath))) {
        qWarning() << "Scan folder" << parser.value(scanDirPath) << "does not exist.";
        return 0;
    }

    QStringList endingsList = parser.value(fileEndings).split(',');
    for(QString& fileEnding : endingsList) {
        fileEnding.prepend("*.");
    }

    qDebug() << "Scanning folder for files with the following endings:" << endingsList;

    // Scan specified dir for image files recursivley
    QFileInfoList fileInfoList = scanDir(parser.value(scanDirPath),
                                         endingsList);

    qDebug() << "Read" << fileInfoList.size() << "image files from" << parser.value(scanDirPath);

    copy(fileInfoList,
         parser.value(targetDirPath),
         parser.value(removeAfterCopy).toInt());

    return a.quit();
}
