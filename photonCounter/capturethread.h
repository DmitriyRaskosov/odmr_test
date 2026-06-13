#ifndef CAPTURETHREAD_H
#define CAPTURETHREAD_H

#include <QThread>
#include <QString>
#include <atomic>

class CaptureThread : public QThread
{
    Q_OBJECT
public:
    explicit CaptureThread(const QString &iface, QObject *parent = nullptr);
    ~CaptureThread();

    void stop();

signals:
    void packetCountPair(int countFirst,int countSecond); // количество пакетов за последние 100 мс

protected:
    void run() override;

private:
    std::atomic<bool> running;
    QString ifaceName;
    int sock_fd;
    void *ring;
    size_t ring_len;
    int frame_size;
    int frame_nr;
    static constexpr int PHOTONS_IN_PACKET = 255;
};

#endif // CAPTURETHREAD_H