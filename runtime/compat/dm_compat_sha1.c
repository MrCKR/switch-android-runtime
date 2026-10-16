#include <CommonCrypto/CommonDigestSHA1.h>
#include <string.h>

static inline uint32_t s1rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static void sha1_block(CC_SHA1_CTX *c, const uint8_t *p) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | p[i*4+3];
    for (int i = 16; i < 80; i++) w[i] = s1rol(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    uint32_t a=c->h[0], b=c->h[1], d=c->h[3], e=c->h[4], x=c->h[2];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f=(b&x)|((~b)&d); k=0x5a827999u; }
        else if (i < 40) { f=b^x^d; k=0x6ed9eba1u; }
        else if (i < 60) { f=(b&x)|(b&d)|(x&d); k=0x8f1bbcdcu; }
        else { f=b^x^d; k=0xca62c1d6u; }
        uint32_t t=s1rol(a,5)+f+e+k+w[i]; e=d; d=x; x=s1rol(b,30); b=a; a=t;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=x; c->h[3]+=d; c->h[4]+=e;
}

void CC_SHA1_Init(CC_SHA1_CTX *c) {
    c->h[0]=0x67452301u; c->h[1]=0xefcdab89u; c->h[2]=0x98badcfeu;
    c->h[3]=0x10325476u; c->h[4]=0xc3d2e1f0u; c->bits=0; c->n=0;
}
void CC_SHA1_Update(CC_SHA1_CTX *c, const void *data, CC_LONG len) {
    const uint8_t *p=data; c->bits += (uint64_t)len*8;
    if (c->n) { size_t take=64-c->n; if (take>len) take=len; memcpy(c->buf+c->n,p,take); c->n+=take; p+=take; len-=(CC_LONG)take; if(c->n==64){sha1_block(c,c->buf);c->n=0;} }
    while (len >= 64) { sha1_block(c,p); p+=64; len-=64; }
    if (len) { memcpy(c->buf,p,len); c->n=len; }
}
void CC_SHA1_Final(uint8_t *out, CC_SHA1_CTX *c) {
    uint64_t bits=c->bits; uint8_t pad[72]={0}; size_t n=c->n<56?56-c->n:120-c->n; pad[0]=0x80;
    for(int i=0;i<8;i++) pad[n+i]=(uint8_t)(bits>>(56-8*i));
    CC_SHA1_Update(c,pad,(CC_LONG)(n+8));
    for(int i=0;i<5;i++) for(int j=0;j<4;j++) out[i*4+j]=(uint8_t)(c->h[i]>>(24-8*j));
}
uint8_t *CC_SHA1(const void *data, CC_LONG len, uint8_t *out) { CC_SHA1_CTX c; CC_SHA1_Init(&c); CC_SHA1_Update(&c,data,len); CC_SHA1_Final(out,&c); return out; }
