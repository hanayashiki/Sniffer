#include "stdafx.h"

#define HAVE_REMOTE
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "pcap.h"
#include "remote-ext.h"
#include <stdlib.h>
#include <stdio.h>
#include <pcap.h> 
#include <remote-ext.h>        //winpcap的头文件
#include <winsock2.h>
#include <process.h>              //多线程编程的头文件
#include <windows.h>
#include <Iphlpapi.h>             //提取网关用的头文件
#pragma comment(lib,"ws2_32")
#pragma comment(lib,"wpcap")
#pragma comment(lib,"IPHlpApi")
#define LINE_LEN 16
#define outPutFile ".\\OutPut.txt"	//结果输出的文件
#define MAX_PACK_NUM  1000       //接收的最多报文数
#define MAX_PACK_LEN  65535     //接收的最大IP报文  
#define MAX_ADDR_LEN  16        //点分十进制地址的最大长度  

#define outPutFile ".\\OutPut.txt"	//结果输出的文件
#define SIO_RCVALL              _WSAIOW(IOC_VENDOR,1)  

#define UDP 1
#define TCP 2

//定义IP报文首部
typedef struct _iphdr {
	unsigned char h_lenver;        //4位首部长度+4位IP版本号  
	unsigned char tos;             //8位服务类型TOS  
	unsigned short total_len;      //16位总长度（字节）  
	unsigned short ident;          //16位标识  
	unsigned short frag_and_flags; //3位标志位  
	unsigned char ttl;             //8位生存时间 TTL  
	unsigned char proto;           //8位协议 (TCP, UDP 或其他)  
	unsigned short checksum;       //16位IP首部校验和  
	unsigned int sourceIP;         //32位源IP地址  
	unsigned int destIP;           //32位目的IP地址  
} IP_HEADER;

//定义TCP报文首部
typedef struct _tcphdr {
	unsigned short th_sport;       //16位源端口  
	unsigned short th_dport;       //16位目的端口  
	unsigned int  th_seq;          //32位序列号  
	unsigned int  th_ack;          //32位确认号  
	unsigned char th_lenres;       //4位首部长度/6位保留字  
	unsigned char th_flag;         //6位标志位  
	unsigned short th_win;         //16位窗口大小  
	unsigned short th_sum;         //16位校验和  
	unsigned short th_urp;         //16位紧急数据偏移量  
} TCP_HEADER;

//定义UDP报文首部
typedef struct _udphdr {
	unsigned short uh_sport;    //16位源端口  
	unsigned short uh_dport;    //16位目的端口  
	unsigned short uh_len;      //16位长度  
	unsigned short uh_sum;      //16位校验和  
} UDP_HEADER;

//定义IPv6报文首部
typedef struct _ipv6hdr {                //  bit
	unsigned int ipv6h_ver_traffic_flow; //   0
	unsigned short ipv6h_payload_len;    //   32
	unsigned char ipv6h_next_header;     //   48
	unsigned char ipv6h_hop_limit;       //   56
	char ipv6_src_addr[128 / 8];
	char ipv6_dst_addr[128 / 8];
} IPv6_HEADER;

/* 回调函数原型 */
void packet_handler(u_char *param, const struct pcap_pkthdr *header, const u_char *pkt_data);

int main(int argc, char **argv)
{
	pcap_if_t *alldevs;
	pcap_if_t *d;
	int inum, inum1;
	int i = 0;
	pcap_t *adhandle;
	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_dumper_t *dumpfile;

	u_int netmask;
	struct bpf_program fcode;
	const char * packet_filter = "";

	/* 检查程序输入参数 */
	/*if(argc != 2)
	{
	printf("usage: %s filename", argv[0]);
	return -1;
	}*/

	/* 获取本机设备列表 */
	if (pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		fprintf(stderr, "Error in pcap_findalldevs: %s\n", errbuf);
		exit(1);
	}

	/* 打印列表 */
	for (d = alldevs; d; d = d->next)
	{
		printf("%d. %s", ++i, d->name);
		if (d->description)
			printf(" (%s)\n", d->description);
		else
			printf(" (No description available)\n");
	}

	if (i == 0)
	{
		printf("\nNo interfaces found! Make sure WinPcap is installed.\n");
		return -1;
	}

	printf("Enter the interface number (1-%d):", i);
	scanf_s("%d", &inum);

	if (inum < 1 || inum > i)
	{
		printf("\nInterface number out of range.\n");
		/* 释放列表 */
		pcap_freealldevs(alldevs);
		return -1;
	}
	printf("choose the packet you want to catch.\n");
	printf("1:tcp\n");
	printf("2:udp\n");
	printf("3:arp\n");
	printf("4:ipv6\n");
	printf("5:all\n");
	printf("Enter the number (1-5):");
	scanf_s("%d", &inum1);
	if (inum1 == 1) {
		packet_filter = "(ip or ip6) and tcp";
	}
	else if (inum1 == 2) {
		packet_filter = "(ip or ip6) and udp";
	}
	else if (inum1 == 3) {
		packet_filter = "arp";
	}
	else if (inum1 == 4) {
		packet_filter = "ip6";
	}

	printf("Use filter: %s\n", packet_filter);
	/* 跳转到选中的适配器 */
	for (d = alldevs, i = 0; i< inum - 1; d = d->next, i++);


	/* 打开适配器 */
	if ((adhandle = pcap_open_live(d->name,          // 设备名
		65536,            // 要捕捉的数据包的部分
						  // 65535保证能捕获到不同数据链路层上的每个数据包的全部内容
		PCAP_OPENFLAG_PROMISCUOUS,    // 混杂模式
		1000,             // 读取超时时间
						  // NULL,             // 远程机器验证
		errbuf            // 错误缓冲池
	)) == NULL)
	{
		fprintf(stderr, "\nUnable to open the adapter. %s is not supported by WinPcap\n", d->name);
		/* 释放设备列表 */
		pcap_freealldevs(alldevs);
		return -1;
	}
	if (d->addresses != NULL)
		/* 获得接口第一个地址的掩码 */
		netmask = ((struct sockaddr_in *)(d->addresses->netmask))->sin_addr.S_un.S_addr;
	else
		/* 如果接口没有地址，那么我们假设一个C类的掩码 */
		netmask = 0xffffff;
	/* 打开堆文件 */
	dumpfile = pcap_dump_open(adhandle, outPutFile);

	if (dumpfile == NULL)
	{
		fprintf(stderr, "\nError opening output file\n");
		return -1;
	}
	/////
	if (pcap_compile(adhandle, &fcode, packet_filter, 1, netmask) <0)
	{
		fprintf(stderr, "\nUnable to compile the packet filter. Check the syntax.\n");
		/* 释放设备列表 */
		pcap_freealldevs(alldevs);
		return -1;
	}

	//设置过滤器
	if (pcap_setfilter(adhandle, &fcode)<0)
	{
		fprintf(stderr, "\nError setting the filter.\n");
		/* 释放设备列表 */
		pcap_freealldevs(alldevs);
		return -1;
	}/////
	printf("\nlistening on %s... Press Ctrl+C to stop...\n", d->description);

	/* 释放设备列表 */
	pcap_freealldevs(alldevs);

	/* 开始捕获 */
	pcap_loop(adhandle, 0, packet_handler, (unsigned char *)dumpfile);

	return 0;
}

/* 回调函数，用来处理数据包 */
void packet_handler(u_char *dumpfile, const struct pcap_pkthdr *header, const u_char *pkt_data)
{

	/* 保存数据包到堆文件 */
	int i;



	IP_HEADER *pip;

	const char *protocol;
	u_int ip_len;
	u_short sport, dport;

	printf("EtherType: %02x%02x\n", pkt_data[12], pkt_data[13]);
	/*判断是否是IP报文*/
	if (pkt_data[13] == 0 || pkt_data[13] == 0xdd) {

		/* 获得IP数据包头部的位置 */

		/*ipv6: EtherType == 0x0800*/
		TCP_HEADER *ptcp;
		UDP_HEADER *pudp;
		pip = (IP_HEADER *)(pkt_data +
			14); //以太网头部长度

		int transport_layer_protocol = NULL;
		

		if (pkt_data[13] == 0x00) {
			printf("    IP packet\n");

			SOCKADDR_IN addr;
			//输出IP
			addr.sin_addr.s_addr = pip->sourceIP;
			printf("%15s  --> ", inet_ntoa(addr.sin_addr));

			addr.sin_addr.s_addr = pip->destIP;
			printf("%15s  ", inet_ntoa(addr.sin_addr));
			printf("\n");

			ptcp = (TCP_HEADER *)((int)pip + sizeof(IP_HEADER));
			pudp = (UDP_HEADER *)((int)pip + sizeof(IP_HEADER));

			
			if (pip->proto == 6) {
				transport_layer_protocol = TCP;
				printf("    TCP  ");
			}
			if (pip->proto == 17) {
				transport_layer_protocol = UDP;
				printf("    UDP  ");
			}
		}
		/*ipv6: EtherType == 0x86dd*/
		if (pkt_data[13] == 0xdd) {
			printf("    ipv6 packet\n");
			IPv6_HEADER* ipv6h = (IPv6_HEADER *)(pkt_data + 14); // ethernet header offset
			char string_addr_buffer[40];
			ipv6_addr_to_string(ipv6h->ipv6_src_addr, string_addr_buffer);
			printf("    %s --->", string_addr_buffer);
			ipv6_addr_to_string(ipv6h->ipv6_dst_addr, string_addr_buffer);
			printf(" %s\n", string_addr_buffer);
			int hop_limit = ipv6h->ipv6h_hop_limit;
			printf("    Hop Limit: %d\n", hop_limit);
			//printf("    sizeof(IPv6_HEADER) == %d\n", sizeof(IPv6_HEADER));
			ptcp = (TCP_HEADER *)((int)pip + sizeof(IPv6_HEADER));
			pudp = (UDP_HEADER *)((int)pip + sizeof(IPv6_HEADER));
			//printf("    ptcp - pip == %d\n", (int)ptcp - (int)pip);

			if (ipv6h->ipv6h_next_header == 6) {
				transport_layer_protocol = TCP;
				printf("    TCP  ");
			}
			if (ipv6h->ipv6h_next_header == 17) {
				transport_layer_protocol = UDP;
				printf("    UDP  ");
			}
		}
		


		printf("    port:");
		//输出 端口 和 报文信息
		if (transport_layer_protocol == TCP) { //TCP
			printf("%8d  -->  %8d", ntohs(ptcp->th_sport), ntohs(ptcp->th_dport));
			putchar('\n');
			//printf("sizeof(TCP_HEADER) = %d\n", sizeof(TCP_HEADER));
			//printf("th_sport offset: %d\n", (int)(&(ptcp->th_sport)) - (int)ptcp);
			//printf("sizeof(ptcp->th_sport) = %d\n", sizeof((ptcp->th_sport)));
			if (ntohs(ptcp->th_sport) == 80) {
				printf("    content: \n %s \n", (char*)((int)ptcp + sizeof(TCP_HEADER)));
			}
		}
		else if (transport_layer_protocol == UDP) { //UDP
			printf("%8d  -->  %8d", ntohs(pudp->uh_sport), ntohs(pudp->uh_dport));
			putchar('\n');
		}
		else {
			putchar('\n');
		}
	}
	else if (pkt_data[13] == 6) {
		printf("    arp packet\n");
		
	}

	printf("    SOURCE MAC:");
	for (i = 7; i <= 12; i++)
	{
		printf("%.2x:", pkt_data[i - 1]);
		pip = (IP_HEADER *)(pkt_data +
			14); //以太网头部长度

	}
	printf("-->");
	printf("DEST MAC:");
	for (i = 1; i <= 6; i++)
	{
		printf("%.2x:", pkt_data[i - 1]);

	}

	printf("\n");

	printf("    data length:%d\n", header->len);
	printf("\n\n");
	pcap_dump(dumpfile, header, pkt_data);
}