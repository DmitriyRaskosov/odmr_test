#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#define K (long)1024
#define M (long)1024*K
#define G (long)1024*M
//#define DEBUG_PRINTS
static const char* filenames[] = {"ch0_","ch1_","ch2_"};
static FILE* files[3] = {NULL,NULL,NULL};
#define BLOCK_SIZE      (1<<20)     // 1 MB
#define BLOCK_NR        64
#define FRAME_SIZE      2048
#define FRAME_NR        ((BLOCK_SIZE * BLOCK_NR) / FRAME_SIZE)
#define BUF_SIZE        (long unsigned)(128*M) 
static volatile int keep_running = 1;
static int saved_pkt_num = -1;
static volatile uint64_t total_bytes = 0;
static volatile uint64_t total_packets = 0;
typedef enum channel{
    UNDEFINED=-1,
    CH_0=0,
    CH_1=1,
    CH_2=2
}Channel_t;

void int_handler(int sig)
{
    keep_running = 0;
}
void measure_traffic_speed(int packet_len) {
    static uint64_t last_bytes = 0;
    static uint64_t last_packets = 0;
    static struct timeval last_time = {0, 0};
    struct timeval now;
    double elapsed;

    gettimeofday(&now, NULL);

    total_bytes += packet_len;
    total_packets++;

    // Инициализация при первом вызове
    if (last_time.tv_sec == 0 && last_time.tv_usec == 0) {
        last_time = now;
        last_bytes = total_bytes;
        last_packets = total_packets;
        return;
    }

    elapsed = (now.tv_sec - last_time.tv_sec) + (now.tv_usec - last_time.tv_usec) / 1e6;

    // Выводим скорость каждую секунду
    if (elapsed >= 1.0) {
        uint64_t diff_bytes = total_bytes - last_bytes;
        uint64_t diff_packets = total_packets - last_packets;
        double speed_mbps = (diff_bytes * 8) / elapsed / 1e6;
        double speed_pps = diff_packets / elapsed;

        printf("Traffic: %.2f Mbps, %.0f pkt/s\n", speed_mbps, speed_pps);

        // Сброс для следующего интервала
        last_time = now;
        last_bytes = total_bytes;
        last_packets = total_packets;
    }
}
void open_file(Channel_t ch){
    static int counters[] = {0,0,0};
    char fname[32];  
    snprintf(fname, sizeof(fname), "%s%d.txt", filenames[ch], counters[ch]);
    FILE *log=fopen(fname,"w");
    counters[ch]++;
    if(!log) {
        perror("fopen");
        return;
    }
    files[ch] = log;
}

void parse_timestamps(const unsigned char* packet, char* out, size_t out_size, Channel_t ch) {
    size_t index = 0x2a + 4;
    char* ptr = out;
    size_t remaining = out_size;
    *ptr = '\0';

    while (index + 3 < 1066 && remaining > 1) {
        uint32_t timestamp = ((uint32_t)packet[index] << 24) |
                            ((uint32_t)packet[index+1] << 16) |
                            ((uint32_t)packet[index+2] << 8) |
                            (uint32_t)packet[index+3];
        int nsec = ((timestamp & 0xFFFFFFC0) >> 6) * 5;
        int written;
        if(ch == CH_0 || ch ==CH_1){
            int pics = (timestamp & 0x0000001F) * 185;
            double full_timestamp = (double)nsec / 1e6 + (double)pics / 1e9;
            written = snprintf(ptr, remaining, "%.7f, ", full_timestamp);
        }
        else{
            int front = (timestamp & 0x00000004) ? 1:0;
            double full_timestamp =  (double)nsec / 1e6;
            written = snprintf(ptr, remaining, "%.6f%d, ", full_timestamp,front);
        }
        if (written < 0 || written >= remaining) break;
        ptr += written;
        remaining -= written;
        index += 4;
    }
    if (ptr > out) ptr[-2] = '\0';  // убираем последнюю запятую
    
}

void write_packet_text(uint64_t pkt_num, const unsigned char *packet, int len, Channel_t channel) {
    static char buffers[3][BUF_SIZE];
    static size_t buf_used[3] = {0,0,0};
    static size_t file_bytes[3] = {0,0,0};

    // Проверка места для номера + hex-дампа + перевод строки
    if (buf_used[channel]  + len*2 + 2 > BUF_SIZE) {
        if (!files[channel]) open_file(channel);
        fwrite(buffers[channel], 1, buf_used[channel], files[channel]);
        file_bytes[channel] += buf_used[channel];
        buf_used[channel] = 0;

        // Ротация при превышении лимита
        if (file_bytes[channel] > BUF_SIZE) {
            fclose(files[channel]);
            files[channel] = NULL;
            open_file(channel);
            file_bytes[channel] = 0;
        }
    }

    char ts_buf[8192];
    parse_timestamps(packet, ts_buf, sizeof(ts_buf),channel);
    if (ts_buf[0]) {
        size_t ts_len = strlen(ts_buf);
        if (buf_used[channel] + ts_len + 1 > BUF_SIZE) {
            if (!files[channel]) open_file(channel);
            fwrite(buffers[channel], 1, buf_used[channel], files[channel]);
            file_bytes[channel] += buf_used[channel];
            buf_used[channel] = 0;

            if (file_bytes[channel] > BUF_SIZE) {
                fclose(files[channel]);
                files[channel] = NULL;
                open_file(channel);
                file_bytes[channel] = 0;
            }
        }
        memcpy(buffers[channel] + buf_used[channel], ts_buf, ts_len);
        buf_used[channel] += ts_len;
        buffers[channel][buf_used[channel]++] = '\n';
    }

    // Предварительный сброс при заполнении буфера
    if (buf_used[channel] > BUF_SIZE - 256) {
        if (!files[channel]) open_file(channel);
        fwrite(buffers[channel], 1, buf_used[channel], files[channel]);
        file_bytes[channel] += buf_used[channel];
        buf_used[channel] = 0;

        if (file_bytes[channel] > BUF_SIZE) {
            fclose(files[channel]);
            files[channel] = NULL;
            open_file(channel);
            file_bytes[channel] = 0;
        }
    }
}
int main(int argc,char **argv) {
    if(argc!=2) {
        fprintf(stderr,"Usage: %s <iface> \n",argv[0]);
        return -1;
    }

    signal(SIGINT,int_handler);

    int sock = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
    if(sock==-1) {
        perror("socket");
        return -1;
    }

    struct tpacket_req req;
    memset(&req,0,sizeof(req));

    req.tp_block_size = BLOCK_SIZE;
    req.tp_block_nr   = BLOCK_NR;
    req.tp_frame_size = FRAME_SIZE;
    req.tp_frame_nr   = FRAME_NR;

    if(setsockopt(sock,SOL_PACKET,PACKET_RX_RING,&req,sizeof(req))<0) {
        perror("PACKET_RX_RING");
        return -1;
    }

    unsigned int if_index = if_nametoindex(argv[1]);
    if(!if_index) {
        perror("if_nametoindex");
        return -1;
    }

    struct sockaddr_ll sll;
    memset(&sll,0,sizeof(sll));

    sll.sll_family   = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex  = if_index;

    if(bind(sock,(struct sockaddr*)&sll,sizeof(sll))<0) {
        perror("bind");
        return -1;
    }

    void *ring = mmap(NULL,req.tp_block_size*req.tp_block_nr,PROT_READ|PROT_WRITE,MAP_SHARED,sock,0);

    if(ring==MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    struct pollfd pfd;
    pfd.fd=sock;
    pfd.events=POLLIN;

    printf("Capturing on %s\n",argv[1]);

    unsigned int idx=0;

    while(keep_running) {
        struct tpacket_hdr *hdr = (struct tpacket_hdr*)((char*)ring + idx*FRAME_SIZE);

        if(!(hdr->tp_status & TP_STATUS_USER)) {
            poll(&pfd,1,1000);
            continue;
        }

        unsigned char *packet = (unsigned char*)hdr + hdr->tp_mac;

        int len = hdr->tp_snaplen;

        if(len==1066) {
            int pkt_num=(packet[0x12]<<8)+packet[0x13];
            Channel_t ch = UNDEFINED;
            if(saved_pkt_num>=0) {   
                //Channel_t ch;
                if((packet[0x2a]&0x03)==0x02) ch = CH_2;
                else if((packet[0x2a]&0x03)==0x01) ch = CH_1;
                else if ((packet[0x2a]&0x03)==0x00)ch = CH_0;

                uint16_t diff = pkt_num - saved_pkt_num;
                if(diff!=1 && !(saved_pkt_num==65535 && pkt_num==0)) {
                    printf("WARNING: packet jump %u -> %u\n",saved_pkt_num,pkt_num);
                }
                write_packet_text(pkt_num,packet,len,ch);
                measure_traffic_speed(len);
            }
            saved_pkt_num=pkt_num;
        }

        __sync_synchronize();
        hdr->tp_status = TP_STATUS_KERNEL;

        idx++;
        if(idx>=FRAME_NR)
            idx=0;
    }

    struct tpacket_stats stats;
    socklen_t slen=sizeof(stats);

    if(getsockopt(sock,SOL_PACKET,PACKET_STATISTICS,&stats,&slen)==0) {
        printf("\nPackets: %u\nDrops: %u\n",stats.tp_packets,stats.tp_drops);
    }

    munmap(ring,req.tp_block_size*req.tp_block_nr);
    close(sock);
    execl("/usr/bin/python3", "/usr/bin/python3", "process_timestamps.py", NULL);
    printf("Done\n");
}