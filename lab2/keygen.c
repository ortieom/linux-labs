#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>
#include <cpuid.h>

MD5_CTX* md5handler;
unsigned char md5digest[MD5_DIGEST_LENGTH];
char md5decode[0x21];
char PSN[0x11];

void calc_md5(void* data, size_t length) {
    MD5_Init(md5handler);
    MD5_Update(md5handler, data, length);
    MD5_Final(md5digest, md5handler);
}

int main() {
    unsigned int cpu_info[9] = {0};
    
    md5handler = (MD5_CTX*)malloc(sizeof(MD5_CTX));
    if (!md5handler) {
        fprintf(stderr, "Memory allocation failed\n");
        return EXIT_FAILURE;
    }
    
    __get_cpuid(1, &cpu_info[0], &cpu_info[1], &cpu_info[2], &cpu_info[3]);
    
    cpu_info[7] = cpu_info[0] << 0x18 |
                  cpu_info[0] >> 0x18 | (cpu_info[0] & 0xff00) << 8 | cpu_info[0] >> 8 & 0xff00;
    cpu_info[8] = cpu_info[3] << 0x18 |
                  cpu_info[3] >> 0x18 | (cpu_info[3] & 0xff00) << 8 | cpu_info[3] >> 8 & 0xff00;
    
    snprintf(PSN, sizeof(PSN), "%08X%08X", cpu_info[7], cpu_info[8]);
    
    calc_md5(PSN, 16);
    
    for (int i = 0; i < 16; i++) {
        sprintf(&md5decode[i * 2], "%02x", (unsigned char)md5digest[15 - i]);
    }
    
    md5decode[32] = '\0';
    
    printf("HWID: %s\n", PSN);
    printf("License key: %s\n", md5decode);
    
    free(md5handler);
    
    return EXIT_SUCCESS;
}
