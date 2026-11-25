#include <OpenWithApplication.h>

#include <QMetaMethod>

bool OpenWithApplication::event(QEvent* event) {
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent* openEvent = static_cast<QFileOpenEvent*>(event);
#ifdef NIGHT_P
        // a custom URL scheme (sioyek://...) arrives as a FileOpen event with an empty file()
        if (openEvent->file().isEmpty() && openEvent->url().scheme().compare("sioyek", Qt::CaseInsensitive) == 0) {
            if (isSignalConnected(QMetaMethod::fromSignal(&OpenWithApplication::url_ready))) {
                emit url_ready(openEvent->url());
            }
            else {
                pending_urls.append(openEvent->url());
            }
            return true;
        }
#endif
        emit file_ready(openEvent->file());
    }

    return QApplication::event(event);
}

QList<QUrl> OpenWithApplication::take_pending_urls() {
    QList<QUrl> res;
    res.swap(pending_urls);
    return res;
}
