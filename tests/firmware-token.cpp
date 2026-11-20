#include "../firmware/attendance-token.h"
#ifdef __APPLE__
#include <CommonCrypto/CommonHMAC.h>
#else
#include <openssl/hmac.h>
#endif
#include <stdlib.h>
void hmac(const uint8_t* key,size_t kl,const uint8_t* data,size_t dl,uint8_t out[32]) {
#ifdef __APPLE__
 CCHmac(kCCHmacAlgSHA256,key,kl,data,dl,out);
#else
 unsigned int length=32;HMAC(EVP_sha256(),key,(int)kl,data,dl,out,&length);
#endif
}
int main(int argc,char**argv) {
 if(argc!=4)return 1;
 uint8_t key[32];for(int i=0;i<32;i++)key[i]=(uint8_t)i;
 uint8_t digest[32];if(!attendanceDigest(key,32,argv[1],strtoull(argv[2],nullptr,10),hmac,digest))return 2;
 char code[5];attendanceCode(digest,code);printf("%s\n",code);
 for(int i=0;i<32;i++)printf("%02x",digest[i]);printf("\n");
 return strcmp(code,argv[3])?3:0;
}
