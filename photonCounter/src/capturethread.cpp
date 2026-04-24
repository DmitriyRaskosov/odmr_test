#include "capturethread.h"
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <QDebug>
#include <arpa/inet.h>
#define BLOCK_SIZE 4096
#define BLOCK_NR   64
#define FRAME_SIZE 2048
#define FRAME_NR   (BLOCK_NR * (BLOCK_SIZE / FRAME_SIZE))

CaptureThread::CaptureThread(const QString &iface, QObject *parent)
    : QThread(parent), running(false), ifaceName(iface), sock_fd(-1), ring(nullptr)
{
}

CaptureThread::~CaptureThread()
{
    stop();
    if (sock_fd != -1) close(sock_fd);
    if (ring) munmap(ring, ring_len);
}

void CaptureThread::stop()
{
    running = false;
    if (isRunning()) {
        wait();
    }
}

void CaptureThread::run()
{
    // 1. Создаём raw socket
    sock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock_fd == -1) {
        perror("socket");
        return;
    }

    // 2. Настраиваем кольцевой буфер (TPACKET_V1)
    struct tpacket_req req;
    memset(&req, 0, sizeof(req));
    req.tp_block_size = BLOCK_SIZE;
    req.tp_block_nr   = BLOCK_NR;
    req.tp_frame_size = FRAME_SIZE;
    req.tp_frame_nr   = FRAME_NR;

    if (setsockopt(sock_fd, SOL_PACKET, PACKET_RX_RING, &req, sizeof(req)) == -1) {
        perror("setsockopt PACKET_RX_RING");
        close(sock_fd);
        return;
    }

    // 3. Привязываемся к конкретному интерфейсу
    unsigned int if_index = if_nametoindex(ifaceName.toLatin1().data());
    if (!if_index) {
        perror("if_nametoindex");
        close(sock_fd);
        return;
    }

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family   = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex  = if_index;

    if (bind(sock_fd, (struct sockaddr*)&sll, sizeof(sll)) == -1) {
        perror("bind");
        close(sock_fd);
        return;
    }

    // 4. Отображаем кольцо в память
    ring_len = req.tp_block_size * req.tp_block_nr;
    ring = mmap(nullptr, ring_len, PROT_READ | PROT_WRITE, MAP_SHARED, sock_fd, 0);
    if (ring == MAP_FAILED) {
        perror("mmap");
        close(sock_fd);
        return;
    }

    // 5. Цикл захвата с интервалами 100 мс
    running = true;
    struct pollfd pfd;
    pfd.fd = sock_fd;
    pfd.events = POLLIN;

    unsigned int idx = 0;
    auto lastTime = std::chrono::steady_clock::now();
    int packetCounter = 0;

    while (running) {
        int ret = poll(&pfd, 1, 100);

        if (ret > 0 && (pfd.revents & POLLIN)) {
            // Есть данные — обрабатываем пакеты, пока есть
            while (true) {
                struct tpacket_hdr *hdr = (struct tpacket_hdr*)((char*)ring + idx * FRAME_SIZE);
                if (!(hdr->tp_status & TP_STATUS_USER))
                    break; // нет больше пакетов

                packetCounter++;
                __sync_synchronize();
                hdr->tp_status = TP_STATUS_KERNEL;
                idx++;
                if (idx >= FRAME_NR) idx = 0;
            }
        }

        // Проверяем, прошло ли 100 мс
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();
        if (elapsed >= 100) {
            emit packetCount(PHOTONS_IN_PACKET*packetCounter); // отправляем накопленное количество (может быть 0)
            packetCounter = 0;
            lastTime = now;
        }
    }

    // Освобождение ресурсов
    munmap(ring, ring_len);
    close(sock_fd);
}