/* (C) 2017 [anp/hsw]; GPLv2 or later; mailto:sysop@880.ru */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <getopt.h>
#include <time.h>		/* strftime */

/* print value with offset in structure */
#define OFFSET_PRINT(x,y) (size_t)&x.y - (size_t)&x, ntohl(fw_header.y)
#define OFFSET_PRINT_STRING(x,y) (size_t)&x.y - (size_t)&x, fw_header.y

/* заголовок ростелекома имеет дополнительное поле CRC, но пока не нашли, где оно прячется */
#ifdef ROSTELECOM
#define	ADD_DATASIZE	0x4
#else
#define	ADD_DATASIZE	0x0
#endif

#ifndef DEFAULT_PROFILE
#define DEFAULT_PROFILE	"h118n"
#endif

typedef struct fw_header {
	uint32_t fwmagic[4];		// {0x99999999, 0x44444444, 0x55555555, 0xAAAAAAAA}
	uint32_t add_data;		// size of additional data in header (rostelecom == 4)
#ifdef ROSTELECOM
	uint32_t extra_crc;		// maybe CRC for header
#endif
	char padding2[8];
	uint32_t UpgradeKey1;		// HWID
	uint32_t UpgradeKey2;		// HWID
	char version[24];		// like "V2.1.3_ROSCNT5", null-terminated
	uint32_t padding3;
	uint32_t unknown1;		// 0x10000, bootloader? eraseblock? size
	uint32_t padding4;
	// offsets in flash file
	uint32_t kernelLen;
	uint32_t kernelOffset;
	uint32_t kernelCRC;
	uint32_t rootfsLen;
	uint32_t rootfsOffset;
	uint32_t rootfsCRC;
	// mtd partition table
	uint32_t kernelAddr;
	uint32_t kernelSize;
	uint32_t rootfsAddr;
	uint32_t rootfsSize;
	uint32_t kernelAddr2;
	uint32_t kernelSize2;
	uint32_t rootfsAddr2;
	uint32_t rootfsSize2;
	char version_descr[24];		// like "THIS IS H118N VERSION", null-terminated
	uint32_t padding5;
	uint32_t unknown14;		// 0x00000009 (rt) / 0x00000000 (domru)
	char version_short[16];
	uint32_t unknown15;		// 0x00000009
	uint32_t padding6;
	uint32_t headerCRC;		// CRC 0x14-0xb8/0x18-0xba
	char build_date[16];		// like "20160127103743", null-terminated
	uint32_t padding7;
	uint32_t FFpadding1;
	uint32_t FFpadding2;
	char padding10[40];
} fw_header_t;

/* таблица параметров для каждого девайса */
struct dev_defaults {
        char *id;
	uint32_t fwmagic[4];
	uint32_t add_data;
	uint32_t UpgradeKey1;
	uint32_t UpgradeKey2;
	char version[24];
	uint32_t unknown1;
	// mtd partition table
	uint32_t kernelAddr;
	uint32_t kernelSize;
	uint32_t rootfsAddr;
	uint32_t rootfsSize;
	uint32_t kernelAddr2;
	uint32_t kernelSize2;
	uint32_t rootfsAddr2;
	uint32_t rootfsSize2;
	char version_descr[24];
	uint32_t unknown14;
	char version_short[16];
	uint32_t unknown15;
	uint32_t FFpadding1;
	uint32_t FFpadding2;
};

/* predefined device profiles */
static struct dev_defaults defaults[] = {
        {
                .id             = "h118n",	// H118N - DOM(RU).RU
		.fwmagic	= {0x99999999, 0x44444444, 0x55555555, 0xAAAAAAAA},
		.add_data	= ADD_DATASIZE,
		.UpgradeKey1	= 0x264,
		.UpgradeKey2	= 0x1,
		.version	= "V2.1.2_ERT5",
		.kernelAddr	= 0x005e0000,	// mtd4
		.kernelSize	= 0x00260000,
		.rootfsAddr	= 0x00080000,	// mtd3
		.rootfsSize	= 0x00560000,
		.kernelAddr2	= 0x00840000,	// mtd5
		.kernelSize2	= 0x00260000,
		.rootfsAddr2	= 0x00aa0000,	// mtd6
		.rootfsSize2	= 0x00560000,
		.version_descr	= "THIS IS H118N VERSION",
		.version_short	= "V2.3",
		.unknown1	= 0x10000,
		.unknown14	= 0x0,
		.unknown15	= 0x09,
		.FFpadding1	= 0xFFFFFFFF,
		.FFpadding2	= 0xFFFFFFFF,
        }, {
                .id             = "h108l-ut",	// H108L - ukrtelecom adsl
		.fwmagic	= {0x99999999, 0x44444444, 0x55555555, 0xAAAAAAAA},
		.add_data	= ADD_DATASIZE,
		.UpgradeKey1	= 0x3,
		.UpgradeKey2	= 0x1,
		.version	= "V1.0.00_UKR",
		.kernelAddr	= 0x00040000,	// mtd4
		.kernelSize	= 0x000d0000,
		.rootfsAddr	= 0x00110000,	// mtd3
		.rootfsSize	= 0x002e0000,
		.kernelAddr2	= 0x0,	// mtd5
		.kernelSize2	= 0x0,
		.rootfsAddr2	= 0x0,	// mtd6
		.rootfsSize2	= 0x0,
		.version_descr	= "THIS IS H108L VERSION",
		.version_short	= "",
		.unknown1	= 0x0,
		.unknown14	= 0x08,
		.unknown15	= 0x01,
		.FFpadding1	= 0xFFFFFFFF,
		.FFpadding2	= 0xFFFFFFFF,
        }, {

                /* terminating entry */
        }
};

static struct dev_defaults *find_defaults(char *id)
{
        struct dev_defaults *ret;
        struct dev_defaults *l;
        
        ret = NULL;
        for (l = defaults; l->id != NULL; l++){
                if (strcasecmp(id, l->id) == 0) {
                        ret = l;
                        break;
                }
        };
        
        return ret;
};

/* crc32 в понимании товарищей из zte */
static unsigned char zte_crc32_table[] = {
0x00,0x00,0x00,0x00, 0x77,0x07,0x30,0x96, 0xee,0x0e,0x61,0x2c, 0x99,0x09,0x51,0xba, 0x07,0x6d,0xc4,0x19, 0x70,0x6a,0xf4,0x8f,
0xe9,0x63,0xa5,0x35, 0x9e,0x64,0x95,0xa3, 0x0e,0xdb,0x88,0x32, 0x79,0xdc,0xb8,0xa4, 0xe0,0xd5,0xe9,0x1e, 0x97,0xd2,0xd9,0x88,
0x09,0xb6,0x4c,0x2b, 0x7e,0xb1,0x7c,0xbd, 0xe7,0xb8,0x2d,0x07, 0x90,0xbf,0x1d,0x91, 0x1d,0xb7,0x10,0x64, 0x6a,0xb0,0x20,0xf2,
0xf3,0xb9,0x71,0x48, 0x84,0xbe,0x41,0xde, 0x1a,0xda,0xd4,0x7d, 0x6d,0xdd,0xe4,0xeb, 0xf4,0xd4,0xb5,0x51, 0x83,0xd3,0x85,0xc7,
0x13,0x6c,0x98,0x56, 0x64,0x6b,0xa8,0xc0, 0xfd,0x62,0xf9,0x7a, 0x8a,0x65,0xc9,0xec, 0x14,0x01,0x5c,0x4f, 0x63,0x06,0x6c,0xd9,
0xfa,0x0f,0x3d,0x63, 0x8d,0x08,0x0d,0xf5, 0x3b,0x6e,0x20,0xc8, 0x4c,0x69,0x10,0x5e, 0xd5,0x60,0x41,0xe4, 0xa2,0x67,0x71,0x72,
0x3c,0x03,0xe4,0xd1, 0x4b,0x04,0xd4,0x47, 0xd2,0x0d,0x85,0xfd, 0xa5,0x0a,0xb5,0x6b, 0x35,0xb5,0xa8,0xfa, 0x42,0xb2,0x98,0x6c,
0xdb,0xbb,0xc9,0xd6, 0xac,0xbc,0xf9,0x40, 0x32,0xd8,0x6c,0xe3, 0x45,0xdf,0x5c,0x75, 0xdc,0xd6,0x0d,0xcf, 0xab,0xd1,0x3d,0x59,
0x26,0xd9,0x30,0xac, 0x51,0xde,0x00,0x3a, 0xc8,0xd7,0x51,0x80, 0xbf,0xd0,0x61,0x16, 0x21,0xb4,0xf4,0xb5, 0x56,0xb3,0xc4,0x23,
0xcf,0xba,0x95,0x99, 0xb8,0xbd,0xa5,0x0f, 0x28,0x02,0xb8,0x9e, 0x5f,0x05,0x88,0x08, 0xc6,0x0c,0xd9,0xb2, 0xb1,0x0b,0xe9,0x24,
0x2f,0x6f,0x7c,0x87, 0x58,0x68,0x4c,0x11, 0xc1,0x61,0x1d,0xab, 0xb6,0x66,0x2d,0x3d, 0x76,0xdc,0x41,0x90, 0x01,0xdb,0x71,0x06,
0x98,0xd2,0x20,0xbc, 0xef,0xd5,0x10,0x2a, 0x71,0xb1,0x85,0x89, 0x06,0xb6,0xb5,0x1f, 0x9f,0xbf,0xe4,0xa5, 0xe8,0xb8,0xd4,0x33,
0x78,0x07,0xc9,0xa2, 0x0f,0x00,0xf9,0x34, 0x96,0x09,0xa8,0x8e, 0xe1,0x0e,0x98,0x18, 0x7f,0x6a,0x0d,0xbb, 0x08,0x6d,0x3d,0x2d,
0x91,0x64,0x6c,0x97, 0xe6,0x63,0x5c,0x01, 0x6b,0x6b,0x51,0xf4, 0x1c,0x6c,0x61,0x62, 0x85,0x65,0x30,0xd8, 0xf2,0x62,0x00,0x4e,
0x6c,0x06,0x95,0xed, 0x1b,0x01,0xa5,0x7b, 0x82,0x08,0xf4,0xc1, 0xf5,0x0f,0xc4,0x57, 0x65,0xb0,0xd9,0xc6, 0x12,0xb7,0xe9,0x50,
0x8b,0xbe,0xb8,0xea, 0xfc,0xb9,0x88,0x7c, 0x62,0xdd,0x1d,0xdf, 0x15,0xda,0x2d,0x49, 0x8c,0xd3,0x7c,0xf3, 0xfb,0xd4,0x4c,0x65,
0x4d,0xb2,0x61,0x58, 0x3a,0xb5,0x51,0xce, 0xa3,0xbc,0x00,0x74, 0xd4,0xbb,0x30,0xe2, 0x4a,0xdf,0xa5,0x41, 0x3d,0xd8,0x95,0xd7,
0xa4,0xd1,0xc4,0x6d, 0xd3,0xd6,0xf4,0xfb, 0x43,0x69,0xe9,0x6a, 0x34,0x6e,0xd9,0xfc, 0xad,0x67,0x88,0x46, 0xda,0x60,0xb8,0xd0,
0x44,0x04,0x2d,0x73, 0x33,0x03,0x1d,0xe5, 0xaa,0x0a,0x4c,0x5f, 0xdd,0x0d,0x7c,0xc9, 0x50,0x05,0x71,0x3c, 0x27,0x02,0x41,0xaa,
0xbe,0x0b,0x10,0x10, 0xc9,0x0c,0x20,0x86, 0x57,0x68,0xb5,0x25, 0x20,0x6f,0x85,0xb3, 0xb9,0x66,0xd4,0x09, 0xce,0x61,0xe4,0x9f,
0x5e,0xde,0xf9,0x0e, 0x29,0xd9,0xc9,0x98, 0xb0,0xd0,0x98,0x22, 0xc7,0xd7,0xa8,0xb4, 0x59,0xb3,0x3d,0x17, 0x2e,0xb4,0x0d,0x81,
0xb7,0xbd,0x5c,0x3b, 0xc0,0xba,0x6c,0xad, 0xed,0xb8,0x83,0x20, 0x9a,0xbf,0xb3,0xb6, 0x03,0xb6,0xe2,0x0c, 0x74,0xb1,0xd2,0x9a,
0xea,0xd5,0x47,0x39, 0x9d,0xd2,0x77,0xaf, 0x04,0xdb,0x26,0x15, 0x73,0xdc,0x16,0x83, 0xe3,0x63,0x0b,0x12, 0x94,0x64,0x3b,0x84,
0x0d,0x6d,0x6a,0x3e, 0x7a,0x6a,0x5a,0xa8, 0xe4,0x0e,0xcf,0x0b, 0x93,0x09,0xff,0x9d, 0x0a,0x00,0xae,0x27, 0x7d,0x07,0x9e,0xb1,
0xf0,0x0f,0x93,0x44, 0x87,0x08,0xa3,0xd2, 0x1e,0x01,0xf2,0x68, 0x69,0x06,0xc2,0xfe, 0xf7,0x62,0x57,0x5d, 0x80,0x65,0x67,0xcb,
0x19,0x6c,0x36,0x71, 0x6e,0x6b,0x06,0xe7, 0xfe,0xd4,0x1b,0x76, 0x89,0xd3,0x2b,0xe0, 0x10,0xda,0x7a,0x5a, 0x67,0xdd,0x4a,0xcc,
0xf9,0xb9,0xdf,0x6f, 0x8e,0xbe,0xef,0xf9, 0x17,0xb7,0xbe,0x43, 0x60,0xb0,0x8e,0xd5, 0xd6,0xd6,0xa3,0xe8, 0xa1,0xd1,0x93,0x7e,
0x38,0xd8,0xc2,0xc4, 0x4f,0xdf,0xf2,0x52, 0xd1,0xbb,0x67,0xf1, 0xa6,0xbc,0x57,0x67, 0x3f,0xb5,0x06,0xdd, 0x48,0xb2,0x36,0x4b,
0xd8,0x0d,0x2b,0xda, 0xaf,0x0a,0x1b,0x4c, 0x36,0x03,0x4a,0xf6, 0x41,0x04,0x7a,0x60, 0xdf,0x60,0xef,0xc3, 0xa8,0x67,0xdf,0x55,
0x31,0x6e,0x8e,0xef, 0x46,0x69,0xbe,0x79, 0xcb,0x61,0xb3,0x8c, 0xbc,0x66,0x83,0x1a, 0x25,0x6f,0xd2,0xa0, 0x52,0x68,0xe2,0x36,
0xcc,0x0c,0x77,0x95, 0xbb,0x0b,0x47,0x03, 0x22,0x02,0x16,0xb9, 0x55,0x05,0x26,0x2f, 0xc5,0xba,0x3b,0xbe, 0xb2,0xbd,0x0b,0x28,
0x2b,0xb4,0x5a,0x92, 0x5c,0xb3,0x6a,0x04, 0xc2,0xd7,0xff,0xa7, 0xb5,0xd0,0xcf,0x31, 0x2c,0xd9,0x9e,0x8b, 0x5b,0xde,0xae,0x1d,
0x9b,0x64,0xc2,0xb0, 0xec,0x63,0xf2,0x26, 0x75,0x6a,0xa3,0x9c, 0x02,0x6d,0x93,0x0a, 0x9c,0x09,0x06,0xa9, 0xeb,0x0e,0x36,0x3f,
0x72,0x07,0x67,0x85, 0x05,0x00,0x57,0x13, 0x95,0xbf,0x4a,0x82, 0xe2,0xb8,0x7a,0x14, 0x7b,0xb1,0x2b,0xae, 0x0c,0xb6,0x1b,0x38,
0x92,0xd2,0x8e,0x9b, 0xe5,0xd5,0xbe,0x0d, 0x7c,0xdc,0xef,0xb7, 0x0b,0xdb,0xdf,0x21, 0x86,0xd3,0xd2,0xd4, 0xf1,0xd4,0xe2,0x42,
0x68,0xdd,0xb3,0xf8, 0x1f,0xda,0x83,0x6e, 0x81,0xbe,0x16,0xcd, 0xf6,0xb9,0x26,0x5b, 0x6f,0xb0,0x77,0xe1, 0x18,0xb7,0x47,0x77,
0x88,0x08,0x5a,0xe6, 0xff,0x0f,0x6a,0x70, 0x66,0x06,0x3b,0xca, 0x11,0x01,0x0b,0x5c, 0x8f,0x65,0x9e,0xff, 0xf8,0x62,0xae,0x69,
0x61,0x6b,0xff,0xd3, 0x16,0x6c,0xcf,0x45, 0xa0,0x0a,0xe2,0x78, 0xd7,0x0d,0xd2,0xee, 0x4e,0x04,0x83,0x54, 0x39,0x03,0xb3,0xc2,
0xa7,0x67,0x26,0x61, 0xd0,0x60,0x16,0xf7, 0x49,0x69,0x47,0x4d, 0x3e,0x6e,0x77,0xdb, 0xae,0xd1,0x6a,0x4a, 0xd9,0xd6,0x5a,0xdc,
0x40,0xdf,0x0b,0x66, 0x37,0xd8,0x3b,0xf0, 0xa9,0xbc,0xae,0x53, 0xde,0xbb,0x9e,0xc5, 0x47,0xb2,0xcf,0x7f, 0x30,0xb5,0xff,0xe9,
0xbd,0xbd,0xf2,0x1c, 0xca,0xba,0xc2,0x8a, 0x53,0xb3,0x93,0x30, 0x24,0xb4,0xa3,0xa6, 0xba,0xd0,0x36,0x05, 0xcd,0xd7,0x06,0x93,
0x54,0xde,0x57,0x29, 0x23,0xd9,0x67,0xbf, 0xb3,0x66,0x7a,0x2e, 0xc4,0x61,0x4a,0xb8, 0x5d,0x68,0x1b,0x02, 0x2a,0x6f,0x2b,0x94,
0xb4,0x0b,0xbe,0x37, 0xc3,0x0c,0x8e,0xa1, 0x5a,0x05,0xdf,0x1b, 0x2d,0x02,0xef,0x8d
};

static uint32_t be2int(unsigned char *c) {
  return c[3] | (c[2] << 8) | (c[1] << 16) | c[0] <<24;
}

uint32_t zte_crc32(unsigned char *src_mem, size_t input_size) {

    /* calculate */
    uint32_t crc = 0xffffffff;

    size_t cur_ptr;
    for(cur_ptr=0; cur_ptr < input_size; cur_ptr++) {
      crc = be2int(zte_crc32_table + (((src_mem[cur_ptr] ^ crc) & 0xFF) << 2)) ^ crc >> 8;
    }

    return ~crc;
};


void header_print(fw_header_t fw_header) {
    printf("Header information (values printed in host byte order):\n");
    printf("-- Device information:\n");
    printf("0x%04x, Size of additional data stored: 0x%x\n",   OFFSET_PRINT(fw_header, add_data));
    printf("0x%04x, HWID1/UpgradeKey1             : 0x%x\n",   OFFSET_PRINT(fw_header, UpgradeKey1));
    printf("0x%04x, HWID2/UpgradeKey2             : 0x%x\n",   OFFSET_PRINT(fw_header, UpgradeKey2));
    printf("0x%04x, Firmware version string       : \"%s\"\n", OFFSET_PRINT_STRING(fw_header, version));
#ifdef ROSTELECOM
    printf("0x%04x, Extra vendor CRC              : 0x%08x\n", OFFSET_PRINT(fw_header, extra_crc));
#endif
    printf("-- Flash file offsets:\n");
    printf("0x%04x, Kernel length                 : 0x%x\n",   OFFSET_PRINT(fw_header, kernelLen));
    printf("0x%04x, Kernel offset                 : 0x%x\n",   OFFSET_PRINT(fw_header, kernelOffset));
    printf("0x%04x, Kernel CRC                    : 0x%x\n",   OFFSET_PRINT(fw_header, kernelCRC));
    printf("0x%04x, Rootfs length                 : 0x%x\n",   OFFSET_PRINT(fw_header, rootfsLen));
    printf("0x%04x, Rootfs offset                 : 0x%x\n",   OFFSET_PRINT(fw_header, rootfsOffset));
    printf("0x%04x, Rootfs CRC                    : 0x%08x\n", OFFSET_PRINT(fw_header, rootfsCRC));
    printf("0x%04x, Header CRC                    : 0x%08x\n", OFFSET_PRINT(fw_header, headerCRC));
    printf("-- MTD partition table:\n");
    printf("--- /dev/mtd4\n");
    printf("0x%04x, Primary Kernel Address        : 0x%x\n",   OFFSET_PRINT(fw_header, kernelAddr));
    printf("0x%04x, Primary Kernel Size           : 0x%x\n",   OFFSET_PRINT(fw_header, kernelSize));
    printf("--- /dev/mtd3\n");
    printf("0x%04x, Primary Rootfs Address        : 0x%x\n",   OFFSET_PRINT(fw_header, rootfsAddr));
    printf("0x%04x, Primary Rootfs Size           : 0x%x\n",   OFFSET_PRINT(fw_header, rootfsSize));
    printf("--- /dev/mtd5\n");
    printf("0x%04x, Backup Kernel Address         : 0x%x\n",   OFFSET_PRINT(fw_header, kernelAddr2));
    printf("0x%04x, Backup Kernel Size            : 0x%x\n",   OFFSET_PRINT(fw_header, kernelSize2));
    printf("--- /dev/mtd6\n");
    printf("0x%04x, Backup Rootfs Address         : 0x%x\n",   OFFSET_PRINT(fw_header, rootfsAddr2));
    printf("0x%04x, Backup Rootfs Size            : 0x%x\n",   OFFSET_PRINT(fw_header, rootfsSize2));
    printf("-- General firmware information:\n");
    printf("0x%04x, Firmware additional version   : \"%s\"\n", OFFSET_PRINT_STRING(fw_header, version_descr));
    printf("0x%04x, Firmware short version        : \"%s\"\n", OFFSET_PRINT_STRING(fw_header, version_short));
    printf("0x%04x, Firmware build date           : \"%s\"\n", OFFSET_PRINT_STRING(fw_header, build_date));
};


int sanity_check(fw_header_t fw_header, unsigned char *src_mem, size_t input_size) {
    int retcode = 0;
    printf("-- Data size checks:\n");
    size_t total_data_size = sizeof(fw_header) + ntohl(fw_header.kernelLen) + ntohl(fw_header.rootfsLen);
    uint32_t calc_kernel_crc = htonl(zte_crc32(src_mem + ntohl(fw_header.kernelOffset), ntohl(fw_header.kernelLen)));
    uint32_t calc_rootfs_crc = htonl(zte_crc32(src_mem + ntohl(fw_header.rootfsOffset), ntohl(fw_header.rootfsLen)));
    uint32_t calc_header_crc = htonl(zte_crc32((unsigned char *)&fw_header.padding2, (size_t)&fw_header.headerCRC - (size_t)&fw_header.padding2));
    
    printf("Total data size in firmware ");
    if (total_data_size == input_size) {
	printf("OK        : %zu bytes\n", total_data_size);
    } else {
	printf("MISMATCH  : ***** calculated %zu bytes, filesize %zu bytes\n", total_data_size, input_size);
	retcode++;
    }

    printf("Kernel image ");
    if (ntohl(fw_header.kernelLen) <= ntohl(fw_header.kernelSize) && ntohl(fw_header.kernelLen) <= ntohl(fw_header.kernelSize2)) {
	printf("fits ");
    } else if (ntohl(fw_header.kernelLen) <= ntohl(fw_header.kernelSize) && ntohl(fw_header.kernelSize2 == 0)) {
	printf("fits first (second is zero size) of ");
    } else {
	printf("DOES NOT FIT ");
	retcode++;
    }
    printf("kernel partitions\n");

    printf("RootFS image ");
    if (ntohl(fw_header.rootfsLen) <= ntohl(fw_header.rootfsSize) && ntohl(fw_header.rootfsLen) <= ntohl(fw_header.rootfsSize2)) {
	printf("fits ");
    } else if (ntohl(fw_header.rootfsLen) <= ntohl(fw_header.rootfsSize) && ntohl(fw_header.rootfsSize2 == 0)) {
	printf("fits first (second is zero size) of ");
    } else {
	printf("DOES NOT FIT ");
	retcode++;
    }
    printf("rootfs partitions\n");

    printf("-- CRC calculations:\n");
    printf("Kernel calculated checksum ");
    if (calc_kernel_crc == fw_header.kernelCRC) {
	printf("OK         : 0x%08x\n", ntohl(calc_kernel_crc));
    } else {
	printf("MISMATCH   : ***** calc 0x%08x, header 0x%08x\n", calc_kernel_crc, fw_header.kernelCRC);
	retcode++;
    }
    printf("RootFS calculated checksum ");
    if (calc_rootfs_crc == fw_header.rootfsCRC) {
	printf("OK         : 0x%08x\n", ntohl(calc_rootfs_crc));
    } else {
	printf("MISMATCH   : ***** calc 0x%08x, header 0x%08x\n", calc_rootfs_crc, fw_header.rootfsCRC);
	retcode++;
    }
    printf("Header calculated checksum ");
    if (calc_header_crc == fw_header.headerCRC) {
	printf("OK         : 0x%08x\n", ntohl(calc_header_crc));
    } else {
	printf("MISMATCH   : ***** calc 0x%08x, header 0x%08x\n", calc_header_crc, fw_header.headerCRC);
	retcode++;
    }

    return retcode;
}

int fw_extract(fw_header_t fw_header, unsigned char *src_mem, char kernel_file[], char rootfs_file[]) {
    int retcode=0;
    FILE* fd_kernel = fopen(kernel_file,"wb");
    if (fd_kernel == 0) {
        perror("Cannot open kernel file for write");
        retcode+=10;
	goto open_fail;
    }
    fseek(fd_kernel, 0, SEEK_SET);
    fwrite(src_mem + ntohl(fw_header.kernelOffset), ntohl(fw_header.kernelLen), 1, fd_kernel);
    fclose(fd_kernel);

    FILE* fd_rootfs = fopen(rootfs_file,"wb");
    if (fd_rootfs == 0) {
        perror("Cannot open rootfs file for write");
        retcode+=10;
	goto open_fail;
    }
    fseek(fd_rootfs, 0, SEEK_SET);
    fwrite(src_mem + ntohl(fw_header.rootfsOffset), ntohl(fw_header.rootfsLen), 1, fd_rootfs);
    fclose(fd_rootfs);
    open_fail:
    return retcode;
}

int main(int argc, char *argv[]) {
    char *input_name=NULL;
    char *input_kernel_name=NULL, *input_rootfs_name=NULL, *output_name=NULL;
    char *profile_name=NULL;
    int retcode=0;
    uint32_t upgradekey1=0, upgradekey2=0;
    int opt_info=0, opt_extract=0, opt_assemble=0;
    int opt_upgradekey1=0, opt_upgradekey2=0;

    fw_header_t fw_header;
    profile_name = DEFAULT_PROFILE;

    int c;
    while ( 1 ) {
        c = getopt(argc, argv, "1:2:p:i:x:k:r:o:n");
        if (c == -1)
                break;

        switch (c) {
                case 'i':  // print info
			input_name = optarg;
                        opt_info++;
			break;
                case 'x':  // extract
			input_name = optarg;
                        opt_extract++;
                        break;
                case 'n':  // assemble new firmware
			input_name = optarg;
                        opt_assemble++;
                        break;
                case 'k':
			input_kernel_name = optarg;
                        break;
                case 'r':
			input_rootfs_name = optarg;
                        break;
		case '1':
			if (!sscanf(optarg, "%08x", &upgradekey1)) goto print_usage;
			opt_upgradekey1++; // Таким нехитрым образом мы можем установить upgradekey даже в 0, если это нужно
                        break;
		case '2':
			if (!sscanf(optarg, "%08x", &upgradekey2)) goto print_usage;
			opt_upgradekey2++;
                        break;
                case 'p':
			profile_name = optarg;
                        break;
                case 'o':
			output_name = optarg;
                        break;
                default:
                        break;
                }
    }


    /* чтение и разбор прошивки */
    if (opt_info || opt_extract) {
	FILE* source_file = fopen(input_name,"r");
	if (source_file == 0) {
    	    perror("Cannot open file for read");
    	    return 10;
	}
    
	fseek(source_file, 0, SEEK_END);
	size_t input_size = ftell(source_file);

	unsigned char *src_mem = malloc(input_size);
	memset(src_mem, 0, input_size);

	fseek(source_file, 0, SEEK_SET);
	fread(src_mem, input_size, 1, source_file);
	fclose(source_file);

	memcpy(&fw_header, src_mem, sizeof(fw_header));

	header_print(fw_header);
	if ( !(retcode += sanity_check(fw_header, src_mem, input_size))) {
	    if (opt_extract) {
		printf("--\n");
		printf("Extracting kernel and rootfs images...\n");
		retcode += fw_extract(fw_header, src_mem, "kernel.bin", "rootfs.bin");
	    }
	}
	free(src_mem);
    }
    /* создание новой прошивки */    
    else if (opt_assemble && input_kernel_name && input_rootfs_name && output_name) {

	static struct dev_defaults *defaults;
	if( !(defaults=find_defaults(profile_name)) ) {
	    fprintf(stderr, "Specified firmware profile (%s) not found!\n", profile_name);
	    retcode+=10;
	    goto exit_early;
	}

	FILE* kernel_fd = fopen(input_kernel_name,"r");
	if (kernel_fd == 0) {
    	    perror("Cannot open file for read");
	    retcode+=10;
	    goto exit_fclose_kernel_only;
	}
	FILE* rootfs_fd = fopen(input_rootfs_name,"r");
	if (rootfs_fd == 0) {
    	    perror("Cannot open file for read");
	    retcode+=10;
	    goto exit_fclose;
	}
    
	fseek(kernel_fd, 0, SEEK_END);
	fseek(rootfs_fd, 0, SEEK_END);

	/* вот тут нам надо сделать динамическое смещение разделов mtd */
	size_t result_kernel_size = defaults->kernelSize - sizeof(fw_header);

	size_t input_kernel_size = ftell(kernel_fd);
	if (result_kernel_size < input_kernel_size) {
	    fprintf(stderr, "Kernel too big to fit mtd partition (%u < %u)\n", result_kernel_size, input_kernel_size);
	    retcode++;
	    goto exit_fclose;
	}

	size_t input_rootfs_size = ftell(rootfs_fd);
	if (defaults->rootfsSize < input_rootfs_size) {
	    fprintf(stderr, "RootFS too big to fit mtd partition (%u < %u)\n", result_kernel_size, input_kernel_size);
	    retcode++;
	    goto exit_fclose;
	}

	size_t total_size = sizeof(fw_header) + result_kernel_size + input_rootfs_size;
	unsigned char *src_mem = malloc(total_size);
	memset(src_mem, 0xFF, total_size); /* fill with erase byte */

	fseek(kernel_fd, 0, SEEK_SET);
	fseek(rootfs_fd, 0, SEEK_SET);
	if (fread(src_mem + sizeof(fw_header), input_kernel_size, 1, kernel_fd) &&
	    fread(src_mem + sizeof(fw_header) + result_kernel_size, input_rootfs_size, 1, rootfs_fd)) {

	    memset(&fw_header, 0, sizeof(fw_header));

	    /* изменяем дефолтные значения, если нужно */
	    if (opt_upgradekey1) {defaults->UpgradeKey1 = upgradekey1;}
	    if (opt_upgradekey2) {defaults->UpgradeKey2 = upgradekey2;}

	    /* просто заполняем заголовок */
	    memcpy(&fw_header.fwmagic, &defaults->fwmagic, sizeof(fw_header.fwmagic));
            fw_header.add_data		= htonl(defaults->add_data);
            fw_header.UpgradeKey1	= htonl(defaults->UpgradeKey1);
            fw_header.UpgradeKey2	= htonl(defaults->UpgradeKey2);
            fw_header.kernelAddr	= htonl(defaults->kernelAddr);
            fw_header.kernelSize	= htonl(defaults->kernelSize);
            fw_header.rootfsAddr	= htonl(defaults->rootfsAddr);
            fw_header.rootfsSize	= htonl(defaults->rootfsSize);
            fw_header.kernelAddr2	= htonl(defaults->kernelAddr2);
            fw_header.kernelSize2	= htonl(defaults->kernelSize2);
            fw_header.rootfsAddr2	= htonl(defaults->rootfsAddr2);
            fw_header.rootfsSize2	= htonl(defaults->rootfsSize2);

	    /* непонятно, что за значения, надо поправить */
            fw_header.unknown1		= htonl(defaults->unknown1);
            fw_header.unknown14		= htonl(defaults->unknown14);
            fw_header.unknown15		= htonl(defaults->unknown15);
            fw_header.FFpadding1	= defaults->FFpadding1;
            fw_header.FFpadding2	= defaults->FFpadding2;

	    /* всякая разная информация о версиях */
	    strncpy(fw_header.version_descr,	defaults->version_descr,	sizeof(defaults->version_descr));
	    strncpy(fw_header.version_short,	defaults->version_short,	sizeof(defaults->version_short));
	    strncpy(fw_header.version,		defaults->version,		sizeof(defaults->version));

	    /* дата создания прошивки */
	    time_t now = time(NULL);
	    struct tm *t = localtime(&now);
	    strftime(fw_header.build_date, sizeof(fw_header.build_date)-1, "%Y%m%d%H%M%S", t);

	    fw_header.kernelOffset	= htonl(sizeof(fw_header));
//	    fw_header.kernelLen		= htonl(input_kernel_size);
	    fw_header.kernelLen		= htonl(result_kernel_size);
//	    fw_header.rootfsOffset	= htonl(sizeof(fw_header) + input_kernel_size);
	    fw_header.rootfsOffset	= htonl(sizeof(fw_header) + result_kernel_size);
	    fw_header.rootfsLen		= htonl(input_rootfs_size);

	    fw_header.kernelCRC = htonl(zte_crc32(src_mem + ntohl(fw_header.kernelOffset), ntohl(fw_header.kernelLen)));
	    fw_header.rootfsCRC = htonl(zte_crc32(src_mem + ntohl(fw_header.rootfsOffset), ntohl(fw_header.rootfsLen)));
	    fw_header.headerCRC = htonl(zte_crc32((unsigned char *)&fw_header.padding2, (size_t)&fw_header.headerCRC - (size_t)&fw_header.padding2));

	    /* копируем заголовок в общую кучу */
	    memcpy(src_mem, &fw_header, sizeof(fw_header));

	    header_print(fw_header);
	    if ( !(retcode += sanity_check(fw_header, src_mem, total_size))) {

		printf("--\n");
		printf("Making firmware image for profile %s...\n", profile_name);

		FILE* fd_output = fopen(output_name,"wb");
		if (fd_output == 0) {
    		    perror("Cannot open output firmware file for write");
    		    return 10;
		}
		fseek(fd_output, 0, SEEK_SET);
		fwrite(src_mem, total_size, 1, fd_output);
		fclose(fd_output);
	    }
	} else {
	    fprintf(stderr, "Read source files failed");
	    retcode++;
	}

//	exit_free:
	free(src_mem);
	exit_fclose:
	fclose(rootfs_fd);
	exit_fclose_kernel_only:
	fclose(kernel_fd);
	exit_early:;
    } else {
	print_usage:
	fprintf(stderr, "Usage:\n"
	"%s { -x input.bin | -i input.bin | -n -k kernel.bin -r rootfs.bin -o output.bin [-1 0xHWID1] [-2 0xHWID2] [-p profile] }\n",
	argv[0]);
	retcode++;
    }

    return retcode;
};
