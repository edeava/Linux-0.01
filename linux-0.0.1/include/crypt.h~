#ifndef CRYPT_H_INCLUDED
#define CRYPT_H_INCLUDED
#include <string.h>
#include <linux/tty.h>

#ifndef __isdigit
#define __isdigit(x) ((x >= '0') && (x <= '9'))
#define KSHIFT 4
#endif

static void __reverse(char *buf, int len)
{
	int i, j;
	char c;
	for(i = 0, j = len - 1; i < j; ++i, --j)
	{
		c = buf[i];
		buf[i] = buf[j];
		buf[j] = c;
	}
}

static int atoi(const char *buf)
{
	int r = 0, i;
	for(i = 0; __isdigit(buf[i]); ++i)
		r = r * 10 + buf[i] - '0';
	return r;
}

static int itoa(int n, char *buf)
{
	int i = 0, sign;
	if((sign = n) < 0)
		n = -n;
	do
	{
		buf[i++] = n % 10 + '0';
	} while((n /= 10) > 0);

	if(sign < 0)
		buf[i++] = '-';
	buf[i] = '\0';
	__reverse(buf, i);
	return i;
}

static unsigned short hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c;

    return hash % 65530;
}

static int m = 0;
static int n = 0, blockStart = 0, globalTime = 0;
int permutacija[1024];
char kljuc[1024], pomocniKljuc[1024], globalKey[1024];

static void sortKey(char * pKey){
    int i, j;
    
    if(pKey != NULL)
	for(i = 0; i < strlen(pKey); i++){
        	kljuc[i] = pKey[i];
    	}
    else{
	for(i = 0; i < strlen(globalKey); i++){
        	kljuc[i] = globalKey[i];
    	}
    }
    if(blockStart == 0){
	blockInit();
	blockStart = 1;
    }

    for(i = 0; i < strlen(kljuc); i++){
        permutacija[i] = i;
        pomocniKljuc[i] = kljuc[i];
    }
    for(i = 0; i < strlen(kljuc) - 1; i++){
        for(j = 0; j < strlen(kljuc) - i - 1; j++){
            if (pomocniKljuc[j] > pomocniKljuc[j + 1]){
                int t = permutacija[j];
                permutacija[j] = permutacija[j + 1];
                permutacija[j + 1] = t;

                char tmp = pomocniKljuc[j];
                pomocniKljuc[j] = pomocniKljuc[j + 1];
                pomocniKljuc[j + 1] = tmp;
            }
        }
    }
}

static void encrypt(char * tekst){
    m = strlen(kljuc);
    n = (strlen(tekst) / m) + (strlen(tekst) % m > 0);

    int i;
    char r[1024];
    int len = strlen(tekst);
    for(i = 0; i < n*m; i++){
        if(permutacija[i / n] + (i % n) * m > len || tekst[permutacija[i / n] + (i % n) * m] == 0)
            r[i] = ' ';
        else r[i] = tekst[permutacija[i / n] + (i % n) * m];
    }
    r[i] = 0;
    for(i = 0; i < n*m; i++){
        tekst[i] = r[i];
    }
    tekst[i] = 0;
}

static void * decrypt(char * tekst){
    int i, j;
    char r[1024];
    m = strlen(kljuc);
    n = (strlen(tekst) / m) + (strlen(tekst) % m > 0);
    int len = strlen(tekst);

    for(i = 0; i < len; i++){
        r[permutacija[i / n] + (i % n) * m] = tekst[i];
    }

    for(i = 0; i < len; i++){
        tekst[i] = r[i];
    }

}
#endif
