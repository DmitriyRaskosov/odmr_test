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
#define BUF_SIZE        (long unsigned)(256*M) 
static volatile int keep_running = 1;
static int saved_pkt_num = -1;
static volatile unsigned int ms_counter =0;

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
/*char* parse_timestamps(const unsigned char* packet){
    clock_t start = clock();
    size_t index =0x2a+4; //start of timestamps
    char* str=calloc(4*K,1);
    char tmp[32];
    while(index<1066){
        uint32_t timestamp = (packet[index]<<24)|(packet[index+1]<<16)|(packet[index+2]<<8)|(packet[index+3]);
        //printf("%d: %X\n",packet[index-4],timestamp);
        int nsec = ((timestamp&0xFFFFFF00)>>8)*5;
        int pics =(timestamp & 0x000000FF)*185;
        double full_timestamp=100*ms_counter+((double)nsec/1E6)+((double)pics/1E9);
        snprintf(tmp, sizeof(tmp), "%.7f, ", full_timestamp);
        strcat(str,tmp);
        index+=4;
    }
    clock_t end = clock();
    printf("Elapsed time = %f",(float)(end-start)/CLOCKS_PER_SEC);
    exit(0);
    //printf("%s\n",str);
    return str;
}*/
void parse_timestamps(const unsigned char* packet, char* out, size_t out_size) {
    //clock_t start = clock();
    size_t index = 0x2a + 4;
    char* ptr = out;
    size_t remaining = out_size;
    *ptr = '\0';

    while (index + 3 < 1066 && remaining > 1) {
        uint32_t timestamp = ((uint32_t)packet[index] << 24) |
                             ((uint32_t)packet[index+1] << 16) |
                             ((uint32_t)packet[index+2] << 8) |
                             (uint32_t)packet[index+3];
        int nsec = ((timestamp & 0xFFFFFF00) >> 8) * 5;
        int pics = (timestamp & 0x000000FF) * 185;
        double full_timestamp = 100 * ms_counter + (double)nsec / 1e6 + (double)pics / 1e9;

        int written = snprintf(ptr, remaining, "%.7f, ", full_timestamp);
        if (written < 0 || written >= remaining) break;
        ptr += written;
        remaining -= written;
        index += 4;
    }
    //clock_t end = clock();
    if (ptr > out) ptr[-2] = '\0';  // убираем последнюю запятую
    //printf("Elapsed time = %f",(float)(end-start)/CLOCKS_PER_SEC);
    //exit(0);
}
/*void write_packet_text(uint64_t pkt_num, const unsigned char *packet, int len, Channel_t channel) {
    static char buffers[3][BUF_SIZE];
    static size_t buf_used[3] = {0,0,0};
    //parse_timestamps(packet);
    char ts_buf[8192];
    parse_timestamps(packet, ts_buf, sizeof(ts_buf));
    char tmp[32];
    int n;
    n = snprintf(tmp, sizeof(tmp), "%llu: ",(unsigned long long)pkt_num);

    if ((buf_used[channel] + n + len*2 + 2 > BUF_SIZE) ) {
        open_file(channel);
        FILE* fp = files[channel];
        fwrite(buffers[channel],1,buf_used[channel],fp);
        buf_used[channel] = 0;
        printf("written channel %d\n",channel);
        fclose(fp);
    }

    memcpy(buffers[channel]+buf_used[channel],tmp,n);
    buf_used[channel] += n;

    static const char hex[]="0123456789ABCDEF";

    for(int i=0;i<len;i++)
    {
        buffers[channel][buf_used[channel]++] = hex[packet[i]>>4];
        buffers[channel][buf_used[channel]++] = hex[packet[i]&0xF];
        #ifdef DEBUG_PRINTS
            printf("%c%c ", hex[packet[i]>>4],hex[packet[i]&0xF]);
        #endif
    }
    //printf("\n");
    buffers[channel][buf_used[channel]++]='\n';

    if(buf_used[channel] > BUF_SIZE-256)
    {
        open_file(channel);
        FILE* fp = files[channel];
        fwrite(buffers[channel],1,buf_used[channel],fp);
        buf_used[channel]=0;
        printf("written channel %d\n",channel);
        fclose(fp);
    }
}*/
//#define MAX_FILE_SIZE (1 * 1024 * 1024)  

void write_packet_text(uint64_t pkt_num, const unsigned char *packet, int len, Channel_t channel) {
    static char buffers[3][BUF_SIZE];
    static size_t buf_used[3] = {0,0,0};
    static size_t file_bytes[3] = {0,0,0};

    char tmp[32];
    int n = snprintf(tmp, sizeof(tmp), "%llu: ", (unsigned long long)pkt_num);

    // Проверка места для номера + hex-дампа + перевод строки
    if (buf_used[channel] + n + len*2 + 2 > BUF_SIZE) {
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

    // Запись номера пакета
    memcpy(buffers[channel] + buf_used[channel], tmp, n);
    buf_used[channel] += n;

    // Hex-дамп пакета
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < len; i++) {
        buffers[channel][buf_used[channel]++] = hex[packet[i] >> 4];
        buffers[channel][buf_used[channel]++] = hex[packet[i] & 0xF];
    }
    buffers[channel][buf_used[channel]++] = '\n';

    // Добавление строки таймстампов
    char ts_buf[8192];
    parse_timestamps(packet, ts_buf, sizeof(ts_buf));
    if (ts_buf[0]) {
        size_t ts_len = strlen(ts_buf);
        if (buf_used[channel] + ts_len + 1 > BUF_SIZE) {
            if (!files[channel]) open_file(channel);
            fwrite(buffers[channel], 1, buf_used[channel], files[channel]);
            file_bytes[channel] += buf_used[channel];
            buf_used[channel] = 0;

            if (file_bytes[channel] > MAX_FILE_SIZE) {
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

        if (file_bytes[channel] > MAX_FILE_SIZE) {
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
    struct timeval start, end;
    gettimeofday(&start, NULL);
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
                if((packet[0x2a]&0xF0)==0xF0)ms_counter++;
                //printf("ch: %d, byte:%X\n",ch,packet[0x2a]);
                uint16_t diff = pkt_num - saved_pkt_num;
                if(diff!=1 && !(saved_pkt_num==65535 && pkt_num==0)) {
                    printf("WARNING: packet jump %u -> %u\n",saved_pkt_num,pkt_num);
                }
                write_packet_text(pkt_num,packet,len,ch);
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
    gettimeofday(&end, NULL);
    long seconds = (end.tv_sec - start.tv_sec);
    long micros = ((seconds * 1000000) + end.tv_usec) - (start.tv_usec);
    long millis = micros / 1000;
    printf("Elapsed time:\nusing 100ms tag: %f s.\nusing clock(): %f\n",(float)ms_counter/10.0,(float)millis/1000.0);
    printf("Done\n");
}