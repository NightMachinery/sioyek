#ifndef OPEN_WITH_APP_H
#define OPEN_WITH_APP_H

#include <QApplication>
#include <QFileOpenEvent>
#include <QList>
#include <QUrl>

class OpenWithApplication : public QApplication
{
    Q_OBJECT
public:
    OpenWithApplication(int& argc, char** argv)
        : QApplication(argc, argv)
    {
    }

    // sioyek:// URLs that arrived before anything connected to `url_ready`
    // (possible during a cold start); the receiver takes them once connected.
    QList<QUrl> take_pending_urls();

signals:
    void file_ready(const QString& file_name);
    void url_ready(const QUrl& url);

protected:
    bool event(QEvent* event) override;

private:
    QList<QUrl> pending_urls;
};

#endif // OPEN_WITH_APP_H
