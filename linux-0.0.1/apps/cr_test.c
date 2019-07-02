#include <string.h>

inline int printstr(char *s)
{
	return write(0, s, strlen(s));
}

inline int println(char *s)
{
	return printstr(s) + printstr("\n");
}

int m = 0, n = 0;
static int permutacija[1024];
static char pomocniKljuc[1024], kljuc[1024];

void sortKey(){
    int i, j;
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

void encrypt(char * tekst){
    m = strlen(kljuc);
    n = (strlen(tekst) / m) + (strlen(tekst) % m > 0);

    int i;
    sortKey();
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

char * decrypt(char * tekst){
    int i, j;
    char r[1024];
    int len = strlen(tekst);
    for(i = 0; i < len; i++){

        r[permutacija[i / n] + (i % n) * m] = tekst[i];
    }
    for(i = 0; i < len; i++){
        tekst[i] = r[i];
    }

}

int main(int argc, char *argv[])
{
	char tekst[1024];
	println("Key");
	read(0, kljuc, 1024);
	println("Tekst");
	read(0, tekst, 1024);
	encrypt(tekst);
	println(tekst);
	decrypt(tekst);
	println(tekst);
    _exit(0);	
}
