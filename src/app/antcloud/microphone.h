#pragma once
#include <QObject>
#include <QString>
#include <memory>

namespace AntCloud {
class Microphone : public QObject {
    Q_OBJECT
public:
    explicit Microphone(QObject* parent = nullptr);
    ~Microphone() override;
    bool enabled() const;
    QString status() const;
    void setEnabled(bool enabled);
    void startSession(const QString& host);
    void stopSession();
signals:
    void changed();
private:
    class Private;
    std::unique_ptr<Private> d;
};
}
