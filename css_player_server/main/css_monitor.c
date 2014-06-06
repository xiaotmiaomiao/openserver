/* 
 * File: css_monitor.c
 * Author: root
 *
 * Created on April 16, 2014, 6:29 PM
 */
#include "css_monitor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <event2/event_struct.h>

#include "logger.h"
#include "linkedlists.h"

struct event_base* base;
static const char MESSAGE[] = "Hello, World!\n";

static void listener_cb(struct evconnlistener *, evutil_socket_t, 
        struct sockaddr *, int socklen, void *);
static void conn_writecb(struct bufferevent *, void *);
static void conn_eventcb(struct bufferevent *, short, void *);
static void signal_cb(evutil_socket_t, short, void *);

#define RECEPTION_BUFFER_LENGTH 1024 //缓冲大小
#define OFF_SET(i) ((i) % RECEPTION_BUFFER_LENGTH)

#define REORDERING_WINDOW_SIZE RECEPTION_BUFFER_LENGTH/2 //排序窗口大小
#define SEQ_MAX 65536
#define OFF_SET_SEQ(i) ((i) % SEQ_MAX)

//排序窗口使用的变量
char pathname[128] = "/etc/cssplayer/normal.h264";
char sortpathname[128] = "/etc/cssplayer/sort.h264";
FILE *normalfile;
FILE *sortfile;

int max_seq;//uh
int espect_seq;
int time_seq;
int old_espect_seq;
struct timeval order_timer;
int t_Recordering;

int lastseq;

//H264 视频包数据结构体
struct frame_block{
    int seq;
    int ts;
    int tns;
    int datalen;
    int frame_type;
    char *dataptr;
    CSS_LIST_ENTRY(frame_block) frame_block_list;
};
//全局链表
CSS_LIST_HEAD_NOLOCK(,frame_block) framepq;

enum gmp_h264_media_type {
    gmp_h264_media_type_metadata = 0x00, //H264媒体类型
    gmp_h264_media_type_metadata_return = 0x01,//信息返回数据
    gmp_h264_media_type_frame_begin = 0x40,//H264数据开始包
    gmp_h264_media_type_frame_other = 0x41,//H264数据中间包
    gmp_h264_media_type_frame_end = 0x42,//H264数据结束包
    gmp_h264_media_type_frame_only = 0x43 //H264未分数据包
};

struct frame_block **reception_buffer;
CSS_LIST_HEAD_NOLOCK(,frame_block) gm_pack_list;

static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *sa, int socklen, void *user_data)
{
    struct event_base *base = user_data;
    struct bufferevent *bev;

    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
            fprintf(stderr, "Error constructing bufferevent!");
            event_base_loopbreak(base);
            return;
    }
    bufferevent_setcb(bev, NULL, conn_writecb, conn_eventcb, NULL);
    bufferevent_enable(bev, EV_WRITE);
    bufferevent_disable(bev, EV_READ);

    bufferevent_write(bev, MESSAGE, strlen(MESSAGE));
}

static void conn_writecb(struct bufferevent *bev, void *user_data)
{
    struct evbuffer *output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0) {
            printf("flushed answer\n");
            bufferevent_free(bev);
    }
}

static void conn_eventcb(struct bufferevent *bev, short events, void *user_data)
{
    if (events & BEV_EVENT_EOF) {
            printf("Connection closed.\n");
    } else if (events & BEV_EVENT_ERROR) {
           // printf("Got an error on the connection: %s\n", strerror(errno));/*XXX win32*/
    }
    /* None of the other events can happen here, since we haven't enabled
     * timeouts */
    bufferevent_free(bev);
}

static void signal_cb(evutil_socket_t sig, short events, void *user_data)
{
    struct event_base *base = user_data;
    struct timeval delay = { 2, 0 };

    printf("Caught an interrupt signal; exiting cleanly in two seconds.\n");

    event_base_loopexit(base, &delay);
}

void *css_monitor_init(void *data)
{
    int port = 9999;
    struct event_base *base;
    struct evconnlistener *listener;
    struct event *signal_event;
    struct sockaddr_in sin;

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Could not initialize libevent!\n");
        return NULL;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    listener = evconnlistener_new_bind(base, listener_cb, (void *)base,
        LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE, -1,
        (struct sockaddr*)&sin,
        sizeof(sin));

    if (!listener) {
        fprintf(stderr, "Could not create a listener!\n");
        return NULL;
    }

    signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);

    if (!signal_event || event_add(signal_event, NULL)<0) {
            fprintf(stderr, "Could not create/add a signal event!\n");
            return NULL;
    }

    event_base_dispatch(base);

    evconnlistener_free(listener);
    event_free(signal_event);
    event_base_free(base);

    css_log(LOG_NOTICE,"css monitor done!\n");

}

void free_frame(struct frame_block * frame){
    if (frame) {
        if (frame->dataptr) {
            free(frame->dataptr);
        }   
        free(frame);
    }   
}

/*submit the gmh h264 package from old UR to cur UR */
int submit_gmh_h264_package(int old_espect_seq, int espect_seq)
{   
    if (old_espect_seq == espect_seq) return 1;
    long cur_ts = 0;
    int last_seq = 0;
    struct frame_block * first_addr = NULL;
    struct frame_block * frame_piece;
    int res;
    FILE *file = sortfile;

    //上一帧小于当前帧，加入缓冲buff
    if (old_espect_seq < espect_seq) {
        int i = old_espect_seq;
        for (; i<espect_seq; i++) {
            int j = OFF_SET(i);
            if (reception_buffer[j]) {
                CSS_LIST_INSERT_TAIL(&framepq, reception_buffer[j], frame_block_list);
                reception_buffer[j] = 0;
            }
        }
    //上一帧大于当前帧，加入缓冲区
    } else {
        espect_seq += SEQ_MAX;
        int i = old_espect_seq;
        for (; i<espect_seq; i++) {
            int j = OFF_SET_SEQ(i);
            int k = OFF_SET(j);
            if (reception_buffer[k]) {
                CSS_LIST_INSERT_TAIL(&framepq, reception_buffer[k], frame_block_list);
                reception_buffer[k] = 0;
            }
        }
    }   
 
    frame_piece = CSS_LIST_FIRST(&framepq);
 
    while (frame_piece) {
        char *pdata = frame_piece->dataptr;
        int datalen = frame_piece->datalen;
        struct frame_block * new_frame = frame_piece->frame_block_list.next;
        
        if (!(res = fwrite(pdata,sizeof(char),datalen,file))) {
            css_log(LOG_WARNING,"fwrite error :%d , %s, seq %d \n", res, strerror(errno), frame_piece->seq);
        }
        
        if (frame_piece->seq != (lastseq+1)) {
             css_log(LOG_ERROR,"==========\n==========\n=======\n=====\n=================LAST SEQ:%d======CUR SEQ:%d===========\n", lastseq, frame_piece->seq);
        }
        
        css_log(LOG_NOTICE,"GET OUT FRAME SEQ:%d\n", frame_piece->seq);
        
        lastseq = frame_piece->seq;
        
        CSS_LIST_REMOVE(&framepq,frame_piece,frame_block_list);
        free_frame(frame_piece);
        frame_piece = new_frame;  
    }
    
    return 0;
}

int gmh_h264_package_build(char *buf)
{
    int offset_seq = 0;
    struct frame_block *frame_block_ptr = NULL;
    unsigned int seqno, ptrlen, timestamp_s, timestamp_ns;  
    unsigned char *gmpheader = (unsigned char *)buf;
    struct timeval tv_timer;
    
    //解析媒体类型
    if(gmpheader[5] == 0x00) {
        //css_log(LOG_NOTICE, "REC BUF[5] TYEP 0X00 h264\n");
    } else {
        css_log(LOG_ERROR, "REC BUF[5] TYPE NOT h264 %02x\n", gmpheader[5]);
    }
    
    //包号
    seqno = (*(unsigned int *)(gmpheader+11));
    
    offset_seq = OFF_SET(seqno);
    
    //mb mean the bottom of the window                
    int uh_seq = (max_seq - seqno + SEQ_MAX) % SEQ_MAX;
    int win_bottom = (max_seq - REORDERING_WINDOW_SIZE + SEQ_MAX) % SEQ_MAX; 
    int seq_wb = (seqno - win_bottom + SEQ_MAX) % SEQ_MAX;
    int ur_wb = (espect_seq - win_bottom + SEQ_MAX) % SEQ_MAX;
    
   // css_log(LOG_WARNING, "uh_seq:%d, win_bot:%d, seq_wb:%d, ur_wb:%d off_seq:%d\n", uh_seq,win_bottom,seq_wb,ur_wb,offset_seq);
    //css_log(LOG_WARNING, "UH:%d, win:%d, seq_wb:%d, ur_wb:%d\n", uh_seq,win_bottom,seq_wb,ur_wb);
    
    //包长度
    ptrlen = ((*(unsigned int *)(gmpheader+15)));
    //时间戳
    timestamp_s = (*(unsigned int *)(gmpheader+19));
    timestamp_ns = (*(unsigned int *)(gmpheader+23));
    //报文类型
    int p_type;
    //64
    if(gmpheader[6] == 0x40){
        p_type = gmp_h264_media_type_frame_begin;
    //65
    } else if(gmpheader[6] == 0x41) {
        p_type = gmp_h264_media_type_frame_other;
    //66
    } else if(gmpheader[6] == 0x42) {
        p_type = gmp_h264_media_type_frame_end;
    //67
    } else if(gmpheader[6] == 0x43) {
        p_type = gmp_h264_media_type_frame_only;
    } else {
        css_log(LOG_ERROR, "can not parse package type %02x\n", buf[6]);
        return -1;
    }
   // css_log(LOG_NOTICE, "Ptrlen:%d, Time_s:%d, Time_ns:%d, Ptype:%02x, SEQ:%d\n", ptrlen, timestamp_s, timestamp_ns, p_type, seqno);
    
    //落入窗体内的数据包
    if (uh_seq < REORDERING_WINDOW_SIZE && seq_wb < ur_wb) {    
        css_log(LOG_ERROR, "SEQ :%d OUT SORT WINDOWS\n", seqno);
        return 0;
    }
    
    //分配缓冲包数量
    if(!reception_buffer){
        if(!(reception_buffer = calloc(RECEPTION_BUFFER_LENGTH, sizeof(struct frame_block*)))){
            css_log(LOG_ERROR, "calloc reception buffer failed\n");
        }
    }
 
    //分配内存存储buf数据
    if (!reception_buffer[offset_seq]) {                    
        frame_block_ptr = (struct frame_block *)calloc(1, sizeof(struct frame_block));
        if (!frame_block_ptr) {
            css_log(LOG_WARNING, "struct frame_block alloca failed!\n");
            return -1;
        }
        
        frame_block_ptr->seq = seqno; 
        frame_block_ptr->ts = timestamp_s;
        frame_block_ptr->tns = timestamp_ns;
        frame_block_ptr->datalen = ptrlen;
        frame_block_ptr->frame_type = p_type;
        frame_block_ptr->dataptr = (char *)calloc(ptrlen, sizeof(char));
        
        if (!frame_block_ptr->dataptr) {
            css_log(LOG_WARNING, "struct frame_block's dataptr alloca failed!\n");
            free(frame_block_ptr);
            return -1;
        }
        
        char *pdata = (char *)buf; 
        memcpy(frame_block_ptr->dataptr, pdata, ptrlen);

        //
        reception_buffer[offset_seq] = frame_block_ptr;

        //判断是否落入排序窗
        if (uh_seq < REORDERING_WINDOW_SIZE) {

            //seq< uh && seq = ur, put ur~next_ur(Less than uh)
            if (seqno == espect_seq) {
                old_espect_seq = espect_seq;
                int i = OFF_SET_SEQ(espect_seq + 1);
                win_bottom = (max_seq - REORDERING_WINDOW_SIZE + SEQ_MAX) % SEQ_MAX;
                int i_wb = (i - win_bottom + SEQ_MAX) % SEQ_MAX;
                int uh_wb = (max_seq - win_bottom + SEQ_MAX) % SEQ_MAX;
                for (; i_wb <= uh_wb; i++) {                                    
                    int j = OFF_SET_SEQ(i);             
                    int k = OFF_SET(j);
                    if (!reception_buffer[k]) {
                        espect_seq = j;
                        break;
                    }   
                }
                //加入共享缓冲区
                submit_gmh_h264_package(old_espect_seq, espect_seq);
            } 
            //                  
        } else {
            //                  
            int max_tmp = seqno + 1;
            max_seq = OFF_SET_SEQ(max_tmp);

            int uh_ur = (max_seq - espect_seq + SEQ_MAX) % SEQ_MAX;                        
            if (uh_ur < REORDERING_WINDOW_SIZE) {//UR at inside of reordering window

                //seq > uh && seq = ur, put ur~next_ur(Less than uh)
                if (seqno == espect_seq) {                              
                    old_espect_seq = espect_seq;
                    int i = espect_seq + 1;
                    win_bottom = (max_seq - REORDERING_WINDOW_SIZE + SEQ_MAX) % SEQ_MAX;
                    int i_wb = (i - win_bottom + SEQ_MAX) % SEQ_MAX;
                    int uh_wb = (max_seq - win_bottom + SEQ_MAX) % SEQ_MAX;
                    for (; i_wb <= uh_wb; i++) {                                
                        int j = OFF_SET_SEQ(i);
                        int k = OFF_SET(j);
                        if (!reception_buffer[k]) {
                            espect_seq = j;
                            break;
                        }   
                    }
                    //加入共享缓冲区
                    submit_gmh_h264_package(old_espect_seq, espect_seq);
                } 
            } else {//UR at outside of reordering window
                old_espect_seq = espect_seq;
                int i = (max_seq - REORDERING_WINDOW_SIZE + SEQ_MAX) % SEQ_MAX;
                win_bottom = (max_seq - REORDERING_WINDOW_SIZE + SEQ_MAX) % SEQ_MAX;
                int i_wb = (i - win_bottom + SEQ_MAX) % SEQ_MAX;
                int uh_wb = (max_seq - win_bottom + SEQ_MAX) % SEQ_MAX;
                for (; i_wb <= uh_wb; i++) {                            
                    int j = OFF_SET_SEQ(i);
                    int k = OFF_SET(j);
                    if (!reception_buffer[k]) {
                        espect_seq = j;
                        break;
                    }
                }
                //加入共享缓冲区
                submit_gmh_h264_package(old_espect_seq, espect_seq);
            }
        }                   
 
        //                      
        win_bottom = (max_seq - REORDERING_WINDOW_SIZE + SEQ_MAX) % SEQ_MAX;
        int ur_wb = (espect_seq - win_bottom + SEQ_MAX)%SEQ_MAX;

        if (t_Recordering) {
            //          
            win_bottom = (max_seq - REORDERING_WINDOW_SIZE + SEQ_MAX) % SEQ_MAX;
            int ux_wb = (time_seq - win_bottom  + SEQ_MAX) % SEQ_MAX;
            int ur_wb = (espect_seq - win_bottom + SEQ_MAX) % SEQ_MAX;
            int uh_ux = (max_seq - time_seq + SEQ_MAX) % SEQ_MAX;
            if (ur_wb >= ux_wb || (uh_ux >= REORDERING_WINDOW_SIZE && max_seq != time_seq)) {                            
                t_Recordering = 0;
                order_timer.tv_sec = 0;
                order_timer.tv_usec = 0;
                time_seq = 0;
            }
        } else {
            //                  
            win_bottom = (max_seq - REORDERING_WINDOW_SIZE + SEQ_MAX) % SEQ_MAX;
            int ur_wb = (espect_seq - win_bottom + SEQ_MAX)%SEQ_MAX;
            //
            if (ur_wb < REORDERING_WINDOW_SIZE) {
                t_Recordering = 1;
                gettimeofday(&order_timer, NULL);
                //order_timer = time(NULL);
                time_seq = max_seq;
            }
        }           
        
        gettimeofday(&tv_timer, NULL);
        
        if (t_Recordering && (((tv_timer.tv_sec - order_timer.tv_sec) >1) || (((tv_timer.tv_sec - order_timer.tv_sec) == 1)&&((tv_timer.tv_usec - order_timer.tv_usec) >1000)))) {
            old_espect_seq = espect_seq;

            int i = time_seq + 1;
            win_bottom = (max_seq - REORDERING_WINDOW_SIZE + SEQ_MAX) % SEQ_MAX;
            int i_wb = (i - win_bottom + SEQ_MAX) % SEQ_MAX;
            int uh_wb = (max_seq - win_bottom + SEQ_MAX) % SEQ_MAX;
            for (; i_wb <= uh_wb; i++) {                        
                int j = OFF_SET(i);
                if (!reception_buffer[j]) {
                    espect_seq = OFF_SET_SEQ(i);
                    break;
                }   
            }
            //加入共享缓冲区
            submit_gmh_h264_package(old_espect_seq, espect_seq);
            win_bottom = (max_seq - REORDERING_WINDOW_SIZE + SEQ_MAX) % SEQ_MAX;
            int ur_wb =  (espect_seq - win_bottom + SEQ_MAX) % SEQ_MAX;
            if (ur_wb <= REORDERING_WINDOW_SIZE) {
                gettimeofday(&order_timer, NULL);              
                time_seq = max_seq;
            } 
        }                   
    }   
    return 0;
}

void onRead(int iCliFd, short iEvent, void *arg)
{
    int iLen;
    char buf[8192];
    struct event *pEvRead = (struct event*)arg;
    struct sockaddr addrClient;
    socklen_t solen = sizeof(struct sockaddr);
    int res;

    //iLen = recv(iCliFd, buf, 8192, 0);
    iLen = recvfrom(iCliFd, buf, 8192, 0, (struct sockaddr *)&addrClient, &solen);

    if(iLen <= 0)
    {
        css_log(LOG_ERROR, "Client closed\n");
        // event_del(pEvRead);
        // event_free(pEvRead);
        // close(iCliFd);
        return;
    }
    
    if (!(res = fwrite(buf,sizeof(char),iLen,normalfile))) {
        css_log(LOG_WARNING,"fwrite error :%d , %s\n", res, strerror(errno));
    }
    
    gmh_h264_package_build(buf);
     
}

void *css_monitor_udp_init(void *data)
{
    int iSvrFd;
    int flag = 1;
    struct sockaddr_in sSvrAddr;
    struct event evListen;
    //char host[32] = "237.196.234.112";
    char host[32] = "192.168.10.82";
    int port = 8080;
    struct timeval timeout = {1,0};
    struct ip_mreq mreq;
    
    normalfile = fopen(pathname, "at+");
    sortfile = fopen(sortpathname, "at+");
    
    if(!normalfile || !sortfile) {
        css_log(LOG_ERROR, "css monitor udp init open file failed\n");
        return;
    }
    
    if (event_init() == NULL) {
        css_log(LOG_ERROR, "css monitor udp init event failed\n");
        return;
    }
    
    if((iSvrFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
       css_log(LOG_ERROR, "css monitor udp init socket failed\n");
       return;
    }
    
    if (setsockopt(iSvrFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) < 0) {
        css_log(LOG_ERROR, "css monitor udp init setsockopt failed\n");
        return;
    }
    
    if(setsockopt(iSvrFd,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout,sizeof(timeout))<0)
    {
        perror("Reusing ADDR failed");    
        return -1;   
    }
    
    memset(&sSvrAddr, 0, sizeof(sSvrAddr));
    sSvrAddr.sin_family = AF_INET;
    sSvrAddr.sin_addr.s_addr = inet_addr(host);
    sSvrAddr.sin_port = htons(port);
    
    if (bind(iSvrFd, (struct sockaddr*)&sSvrAddr, sizeof(sSvrAddr)) < 0 ) {
        css_log(LOG_ERROR, "css monitor udp init bind failed\n");
        return ;
    } else {
        css_log(LOG_NOTICE, "css monitor udp init bind %s : %d", host, port);
    }
    
 //   mreq.imr_multiaddr.s_addr=inet_addr(host);    
 //   mreq.imr_interface.s_addr=htonl(INADDR_ANY);   
    
 //   if (setsockopt(iSvrFd,IPPROTO_IP,IP_ADD_MEMBERSHIP,(char *)&mreq,sizeof(mreq)) < 0)     
//    {    
 //       perror("setsockopt");  
 //       return -1;    
 //   }
    
    //base = event_base_new();
    event_set(&evListen, iSvrFd, EV_READ|EV_PERSIST, &onRead, NULL);
    
   // event_base_set(base, &evListen);
    if (event_add(&evListen, NULL) == -1) {
        css_log(LOG_ERROR, "css monitor udp init event add failed\n");
    }
   // event_base_dispatch(base);
    
    event_dispatch();
    
    return;
}