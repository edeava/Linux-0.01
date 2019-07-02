/*
 *	console.c
 *
 * This module implements the console io functions
 *	'void con_init(void)'
 *	'void con_write(struct tty_queue * queue)'
 * Hopefully this will be a rather complete VT102 implementation.
 *
 */

/*
 *  NOTE!!! We sometimes disable and enable interrupts for a short while
 * (to put a word in video IO), but this will work even for keyboard
 * interrupts. We know interrupts aren't enabled when getting a keyboard
 * interrupt, as we use trap-gates. Hopefully all is well.
 */

#include <linux/sched.h>
#include <linux/tty.h>
#include <asm/io.h>
#include <asm/system.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <crypt.h>

#define SCREEN_START 0xb8000
#define SCREEN_END   0xc0000
#define LINES 25
#define COLUMNS 80
#define NPAR 16

extern void keyboard_interrupt(void);

static unsigned long origin=SCREEN_START;
static unsigned long scr_end=SCREEN_START+LINES*COLUMNS*2;
static unsigned long pos;
unsigned short fOne = 0;
unsigned short enableWrite = 0;
unsigned short switchTool = 0;
unsigned short highlighted = 278;
char cValues[10][21] = {0};
char eValues[10][100] = {0};
short colours[10] = {0};
char currentDir[128] = "/";
static unsigned long x,y;
static unsigned long top=0,bottom=LINES;
static unsigned long lines=LINES,columns=COLUMNS;
static unsigned long state=0;
static unsigned long npar,par[NPAR];
static unsigned long ques=0;
static unsigned char attr=0x07;

/*
 * this is what the terminal answers to a ESC-Z or csi0c
 * query (= vt100 response).
 */
#define RESPONSE "\033[?1;2c"

static inline void gotoxy(unsigned int new_x,unsigned int new_y)
{
	if (new_x>=columns || new_y>=lines)
		return;
	x=new_x;
	y=new_y;
	pos=origin+((y*columns+x)<<1);
}

static inline void set_origin(void)
{
	cli();
	outb_p(12,0x3d4);
	outb_p(0xff&((origin-SCREEN_START)>>9),0x3d5);
	outb_p(13,0x3d4);
	outb_p(0xff&((origin-SCREEN_START)>>1),0x3d5);
	sti();
}

static void scrup(void)
{
	if (!top && bottom==lines) {
		origin += columns<<1;
		pos += columns<<1;
		scr_end += columns<<1;
		if (scr_end>SCREEN_END) {
			
			int d0,d1,d2,d3;
			__asm__ __volatile("cld\n\t"
				"rep\n\t"
				"movsl\n\t"
				"movl %[columns],%1\n\t"
				"rep\n\t"
				"stosw"
				:"=&a" (d0), "=&c" (d1), "=&D" (d2), "=&S" (d3)
				:"0" (0x0720),
				 "1" ((lines-1)*columns>>1),
				 "2" (SCREEN_START),
				 "3" (origin),
				 [columns] "r" (columns)
				:"memory");

			scr_end -= origin-SCREEN_START;
			pos -= origin-SCREEN_START;
			origin = SCREEN_START;
		} else {
			int d0,d1,d2;
			__asm__ __volatile("cld\n\t"
				"rep\n\t"
				"stosl"
				:"=&a" (d0), "=&c" (d1), "=&D" (d2) 
				:"0" (0x07200720),
				"1" (columns>>1),
				"2" (scr_end-(columns<<1))
				:"memory");
		}
		set_origin();
	} else {
		int d0,d1,d2,d3;
		__asm__ __volatile__("cld\n\t"
			"rep\n\t"
			"movsl\n\t"
			"movl %[columns],%%ecx\n\t"
			"rep\n\t"
			"stosw"
			:"=&a" (d0), "=&c" (d1), "=&D" (d2), "=&S" (d3)
			:"0" (0x0720),
			"1" ((bottom-top-1)*columns>>1),
			"2" (origin+(columns<<1)*top),
			"3" (origin+(columns<<1)*(top+1)),
			[columns] "r" (columns)
			:"memory");
	}
}

static void scrdown(void)
{
	int d0,d1,d2,d3;
	__asm__ __volatile__("std\n\t"
		"rep\n\t"
		"movsl\n\t"
		"addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
		"movl %[columns],%%ecx\n\t"
		"rep\n\t"
		"stosw"
		:"=&a" (d0), "=&c" (d1), "=&D" (d2), "=&S" (d3)
		:"0" (0x0720),
		"1" ((bottom-top-1)*columns>>1),
		"2" (origin+(columns<<1)*bottom-4),
		"3" (origin+(columns<<1)*(bottom-1)-4),
		[columns] "r" (columns)
		:"memory");
}

static void lf(void)
{
	if (y+1<bottom) {
		y++;
		pos += columns<<1;
		return;
	}
	scrup();
}

static void ri(void)
{
	if (y>top) {
		y--;
		pos -= columns<<1;
		return;
	}
	scrdown();
}

static void cr(void)
{
	pos -= x<<1;
	x=0;
}

static void del(void)
{
	if (x) {
		pos -= 2;
		x--;
		*(unsigned short *)pos = 0x0720;
	}
}

static void csi_J(int par)
{
	long count;
	long start;

	switch (par) {
		case 0:	/* erase from cursor to end of display */
			count = (scr_end-pos)>>1;
			start = pos;
			break;
		case 1:	/* erase from start to cursor */
			count = (pos-origin)>>1;
			start = origin;
			break;
		case 2: /* erase whole display */
			count = columns*lines;
			start = origin;
			break;
		default:
			return;
	}
	int d0,d1,d2;
	__asm__ __volatile__("cld\n\t"
		"rep\n\t"
		"stosw\n\t"
		:"=&c" (d0), "=&D" (d1), "=&a" (d2)
		:"0" (count),"1" (start),"2" (0x0720)
		:"memory");
}

static void csi_K(int par)
{
	long count;
	long start;

	switch (par) {
		case 0:	/* erase from cursor to end of line */
			if (x>=columns)
				return;
			count = columns-x;
			start = pos;
			break;
		case 1:	/* erase from start of line to cursor */
			start = pos - (x<<1);
			count = (x<columns)?x:columns;
			break;
		case 2: /* erase whole line */
			start = pos - (x<<1);
			count = columns;
			break;
		default:
			return;
	}
	int d0,d1,d2;
	__asm__ __volatile__("cld\n\t"
		"rep\n\t"
		"stosw\n\t"
		:"=&c" (d0), "=&D" (d1), "=&a" (d2)
		:"0" (count),"1" (start),"2" (0x0720)
		:"memory");
}

void csi_m(void)
{
	int i;

	for (i=0;i<=npar;i++)
		switch (par[i]) {
			case 0:attr=0x07;break;
			case 1:attr=0x0f;break;
			case 4:attr=0x0f;break;
			case 7:attr=0x70;break;
			case 27:attr=0x07;break;
		}
}

static inline void set_cursor(void)
{
	cli();
	outb_p(14,0x3d4);
	outb_p(0xff&((pos-SCREEN_START)>>9),0x3d5);
	outb_p(15,0x3d4);
	outb_p(0xff&((pos-SCREEN_START)>>1),0x3d5);
	sti();
}

static void respond(struct tty_struct * tty)
{
	char * p = RESPONSE;

	cli();
	while (*p) {
		PUTCH(*p,tty->read_q);
		p++;
	}
	sti();
	copy_to_cooked(tty);
}

static void insert_char(void)
{
	int i=x;
	unsigned short tmp,old=0x0720;
	unsigned short * p = (unsigned short *) pos;

	while (i++<columns) {
		tmp=*p;
		*p=old;
		old=tmp;
		p++;
	}
}

static void insert_line(void)
{
	int oldtop,oldbottom;

	oldtop=top;
	oldbottom=bottom;
	top=y;
	bottom=lines;
	scrdown();
	top=oldtop;
	bottom=oldbottom;
}

static void delete_char(void)
{
	int i;
	unsigned short * p = (unsigned short *) pos;

	if (x>=columns)
		return;
	i = x;
	while (++i < columns) {
		*p = *(p+1);
		p++;
	}
	*p=0x0720;
}

static void delete_line(void)
{
	int oldtop,oldbottom;

	oldtop=top;
	oldbottom=bottom;
	top=y;
	bottom=lines;
	scrup();
	top=oldtop;
	bottom=oldbottom;
}

static void csi_at(int nr)
{
	if (nr>columns)
		nr=columns;
	else if (!nr)
		nr=1;
	while (nr--)
		insert_char();
}

static void csi_L(int nr)
{
	if (nr>lines)
		nr=lines;
	else if (!nr)
		nr=1;
	while (nr--)
		insert_line();
}

static void csi_P(int nr)
{
	if (nr>columns)
		nr=columns;
	else if (!nr)
		nr=1;
	while (nr--)
		delete_char();
}

static void csi_M(int nr)
{
	if (nr>lines)
		nr=lines;
	else if (!nr)
		nr=1;
	while (nr--)
		delete_line();
}

static int saved_x=0;
static int saved_y=0;

static void save_cur(void)
{
	saved_x=x;
	saved_y=y;
}

static void restore_cur(void)
{
	x=saved_x;
	y=saved_y;
	pos=origin+((y*columns+x)<<1);
}

//pored ove implementacije, imamo i deklaraciju funkcije u include/linux/tty.h
void prikazi_root_velicinu(void)
{
	struct m_inode *dir_inode;
	struct m_inode *root_inode;
	short* my_vmem_pos;
	char* file_name = "/root"; //ime direktorijuma za koji dohvatamo velicinu
	int xPos = 50;
	int yPos = 6;

	//da bi namei funkcija mogla da obilazi FS, moramo da imamo podesen
	//pokazivac na root inode i na pwd inode
	//posto se ova funkcija poziva unutar kernel-a, oba ova ce biti NULL
	//tako da namei ne bi funkcionisao bez naredne tri linije
	//stvar je u tome da sistem nije dizajniran tako da se unutar kernela radi sa fajlovima
	//mi ovo radimo samo da bismo povezali razlicite komponente u kernelu kao skolski primer
	root_inode = iget(0x301, 1); //dohvatamo prvi inode na prvom disku
	current->root = root_inode;
	current->pwd = root_inode;

	//namei funkcija nam dohvata inode na osnovu putanje
	dir_inode = namei(file_name);
	int size = dir_inode->i_size; //vrednost koju prikazujemo

	my_vmem_pos = origin + COLUMNS * 2 * yPos + xPos * 2; //nasa pozicija u video memoriji
	while (size > 0) {
		int digit = size % 10; //sracunamo najdesniju cifru
		char char_digit = digit + '0';
		*my_vmem_pos = ((short)0x02 << 8) | char_digit; //prikazemo
		my_vmem_pos--; //posto je pokazivac na short, ovo nas vraca dva bajta na levo
		size /= 10; //sledeca cifra
	}

	//svaki iget mora da ima svoj iput, slicno kao sto svaki open mora da ima svoj close
	//iget zapravo belezi dohvaceni inode u internom nizu
	//ako zanemarimo iput, taj niz ce se pre ili kasnije prepuniti
	iput(root_inode);
	//namei je unutar sebe radio iget, tako da i za ovo treba iput
	iput(dir_inode);
	current->root = NULL;
	current->pwd = NULL;
	
}

void con_write(struct tty_struct * tty)
{
	int nr;
	char c;
	nr = CHARS(tty->write_q);
	while (nr--) {
		GETCH(tty->write_q,c);
		switch(state) {
			case 0:
				if (c>31 && c<127) {
					if (x>=columns) {
						x -= columns;
						pos -= columns<<1;
						lf();
					}
					 __asm__("movb attr,%%ah\n\t"
						"movw %%ax,%1\n\t"
						::"a" (c),"m" (*(short *)pos)
						/*:"ax"*/);
					pos += 2;
					x++;
				} else if (c==27)
					state=1;
				else if (c==10 || c==11 || c==12)
					lf();
				else if (c==13)
					cr();
				else if (c==ERASE_CHAR(tty))
					del();
				else if (c==8) {
					if (x) {
						x--;
						pos -= 2;
					}
				} else if (c==9) {
					c=8-(x&7);
					x += c;
					pos += c<<1;
					if (x>columns) {
						x -= columns;
						pos -= columns<<1;
						lf();
					}
					c=9;
				}
				break;
			case 1:
				state=0;
				if (c=='[')
					state=2;
				else if (c=='E')
					gotoxy(0,y+1);
				else if (c=='M')
					ri();
				else if (c=='D')
					lf();
				else if (c=='Z')
					respond(tty);
				else if (x=='7')
					save_cur();
				else if (x=='8')
					restore_cur();
				break;
			case 2:
				for(npar=0;npar<NPAR;npar++)
					par[npar]=0;
				npar=0;
				state=3;
				if ((ques=(c=='?')))
					break;
			case 3:
				if (c==';' && npar<NPAR-1) {
					npar++;
					break;
				} else if (c>='0' && c<='9') {
					par[npar]=10*par[npar]+c-'0';
					break;
				} else state=4;
			case 4:
				state=0;
				switch(c) {
					case 'G': case '`':
						if (par[0]) par[0]--;
						gotoxy(par[0],y);
						break;
					case 'A':
						if (!par[0]) par[0]++;
						gotoxy(x,y-par[0]);
						break;
					case 'B': case 'e':
						if (!par[0]) par[0]++;
						gotoxy(x,y+par[0]);
						break;
					case 'C': case 'a':
						if (!par[0]) par[0]++;
						gotoxy(x+par[0],y);
						break;
					case 'D':
						if (!par[0]) par[0]++;
						gotoxy(x-par[0],y);
						break;
					case 'E':
						if (!par[0]) par[0]++;
						gotoxy(0,y+par[0]);
						break;
					case 'F':
						if (!par[0]) par[0]++;
						gotoxy(0,y-par[0]);
						break;
					case 'd':
						if (par[0]) par[0]--;
						gotoxy(x,par[0]);
						break;
					case 'H': case 'f':
						if (par[0]) par[0]--;
						if (par[1]) par[1]--;
						gotoxy(par[1],par[0]);
						break;
					case 'J':
						csi_J(par[0]);
						break;
					case 'K':
						csi_K(par[0]);
						break;
					case 'L':
						csi_L(par[0]);
						break;
					case 'M':
						csi_M(par[0]);
						break;
					case 'P':
						csi_P(par[0]);
						break;
					case '@':
						csi_at(par[0]);
						break;
					case 'm':
						csi_m();
						break;
					case 'r':
						if (par[0]) par[0]--;
						if (!par[1]) par[1]=lines;
						if (par[0] < par[1] &&
						    par[1] <= lines) {
							top=par[0];
							bottom=par[1];
						}
						break;
					case 's':
						save_cur();
						break;
					case 'u':
						restore_cur();
						break;
				}
		}
	}
	set_cursor();
}

void init_tool(){
	int i = 116;
	int colour;	
	if(fOne) colour = 0x03;
	else colour = 0x07;

	if(switchTool == 0){
		volatile short centre = (44 - strlen(currentDir) * 2) / 2;
		for(; i < 1920; i += 2){
			if((i % 160) >= 116) 
				if((i <= 160) || (i >= 1876) || ((i % 160) == 116) || ((i % 160) == 158))
					if(i >= 116 + centre && i <= 160 - centre){	
						char ch = currentDir[(i % 160 - 116 - centre) / 2];
						*(short*)(origin + i) = ((short)0x07 << 8) | ch;
					}else {	
						*(short*)(origin + i) = ((short)colour << 8) | 0x23;
					}
				else{
					*(short*)(origin + i) = ((short)0x07 << 8) | 0x20;
				}		
		
		}
		from_explorer(currentDir);
		mark(highlighted,1);	
	}
	else{	
		volatile char * naslov = "[ clipboard ]";
		volatile short centre = (44 - strlen(naslov) * 2) / 2;
		for(; i < 1920; i += 2){
			if((i % 160) >= 116) 
				if((i <= 160) || (i >= 1876) || ((i % 160) == 116) || ((i % 160) == 158))
					if(i >= 116 + centre && i <= 160 - centre){	
						char ch = naslov[(i % 160 - 116 - centre) / 2];
						*(short*)(origin + i) = ((short)0x07 << 8) | ch;
					}else {	
						*(short*)(origin + i) = ((short)colour << 8) | 0x23;
					}
				else{
					*(short*)(origin + i) = ((short)0x07 << 8) | 0x20;
				}		
		
		}
		from_clipboard();
		mark(highlighted,1);
	}
	
}

void mark(int start, int way){
	int i = 0, colour;
	if(way) colour = 0x70;
	else colour = 0x07;
	if(switchTool)	
		for(i = 0; i < 40; i += 2){
			char ch = *(short*)(origin + start + i) & 255;
			*(short*)(origin + start + i) = ((short)colour << 8) | ch;	
		}
	else
		for(i = 0; i < 40; i += 2){
			char ch = *(short*)(origin + start + i) & 255;
			*(short*)(origin + start + i) = ((short)colour << 8) | ch;	
		}
}

void go_up(){
	int top = 118, bottom = 118 + 1760;
	if(switchTool == 0){
		int i;
		for(i = 0; i < 10; i++)
			if(eValues[i][0] == 0) break;
		i++;
		bottom = 118 + i * 160;	
	}
	if(highlighted - 160 <= top){
		mark(highlighted,0);
		highlighted = bottom - 160;
		mark(highlighted,1); 	
	}else{
		mark(highlighted, 0);
		highlighted -= 160;
		mark(highlighted,1);
	} 
}

void go_down(){
	int top = 118, bottom = 118 + 1760;
	if(switchTool == 0){
		int i;
		for(i = 0; i < 10; i++)
			if(eValues[i][0] == 0) break;
		i++;
		bottom = 118 + i * 160;	
	}
	if(highlighted + 160 >= bottom){
		mark(highlighted,0);
		highlighted = top + 160;
		mark(highlighted,1); 	
	}else{
		mark(highlighted, 0);
		highlighted += 160;
		mark(highlighted,1);
	} 
}

void from_clipboard(){
	int j = 0, i;	
	for(; j < 10; j++){
		int curr = (j + 1) * 160 + 118;
		int maks = strlen(cValues[j]) >= 20 ? 20 : strlen(cValues[j]);
		short centre = (40 - maks * 2) / 2;

		for(i = 0; i < 40; i += 2){
			if(i >= centre && i <= 40 - centre){
				char chW = cValues[j][(i - centre) / 2];
				*(short*)(origin + curr + i) = ((short)0x07 << 8) | chW;	
			}
			else{
				*(short*)(origin + curr + i) = ((short)0x07 << 8) |0x20;		
			}
		}

	}
}

void clipboard_write(char c){
	int index = highlighted / 160 - 1, i;
	int size = strlen(cValues[index]) >= 20 ? 20 : strlen(cValues[index]);
	if(c == 127 || c == 8){
		size--;
		cValues[index][size] = 0;
	}
	else if(size < 20){
		cValues[index][size] = c;
		size++;
		cValues[index][size] = 0;	
	}

	int maks = strlen(cValues[index]) >= 20 ? 20 : strlen(cValues[index]);
	short centre = (40 - maks * 2) / 2;

	for(i = 0; i < 40; i += 2){
		if(i >= centre && i <= 40 - centre){
			char chW = cValues[index][(i - centre) / 2];
			*(short*)(origin + highlighted + i) = ((short)0x07 << 8) | chW;	
		}
		else{
			*(short*)(origin + highlighted + i) = ((short)0x07 << 8) | 0x20;		
		}
	}

	mark(highlighted, 1);
}

void paste(){
	int index = highlighted / 160 - 1, i, size;
	char * print[128] = {0};
	if(switchTool){
		size = strlen(cValues[index]);
		for(i = 0; i < size; i++)
			PUTCH(cValues[index][i], tty_table[0].read_q); 
	}
	else{
		size = strlen(eValues[index]);
		for(i = 0; i < strlen(currentDir); i++)
			PUTCH(currentDir[i], tty_table[0].read_q); 
		if(currentDir[1])
			PUTCH('/', tty_table[0].read_q); 
		for(i = 0; i < size; i++)
			PUTCH(eValues[index][i], tty_table[0].read_q); 
	}
}

void populate_explorer(char * dir){
	struct m_inode *dir_inode;
	struct m_inode *root_inode;
	struct buffer_head *buff_head;
	struct dir_entry *file;

	root_inode = iget(0x301, 1);
	current->root = root_inode;
	current->pwd = root_inode;

	dir_inode = namei(dir);
	
	if(S_ISDIR(dir_inode->i_mode)){	
		
		struct dirent entry;
		int current_dir_fd;
		int len = 1, index = 0, j, i;
		int size = strlen(dir);


		buff_head = bread(0x301,dir_inode->i_zone[0]);
		file = (struct dir_entry*) buff_head->b_data;
		file += 2;
		while (index < 10 && file->name[0]){
			char target[64] = "";
			strcpy(target, dir);
			if(dir[1])
				strcat(target, "/");
			strcat(target, file->name);
			if(file->name[0] == '.'){
				file++;
				continue;
			}

			colours[index] = 0x07;
			struct m_inode * child_inode;
			child_inode = namei(target);
			if(child_inode->i_mode & 0111)
				colours[index] = 0x02;
			if(S_ISDIR(child_inode->i_mode))
				colours[index] = 0x03;
			if(S_ISCHR(child_inode->i_mode))
				colours[index] = 0x06;
			/*for(i = 0; i < 1024; i++)
				if(child_inode->i_num == encrypted[i])
					colours[index] = 0x04;*/
			iput(child_inode);
			strcpy(eValues[index], file->name);
			index++;
			file++;
		};

		close(current_dir_fd);
		
		for(; index < 10; index++){
			int maks = strlen(eValues[index]);
			for(j = 0; j < maks; j++)
				eValues[index][j] = 0;	
			}
			colours[index] = 0;
			
	}else{
		int i = strlen(dir) - 1;
		for(; i >= 0; i--){
			if(dir[i] == '/')
				break;	
			dir[i] = 0;
		}
		dir[i] = 0;
		dir[i] = 0;
		if(dir[0] == 0)
			dir[0] = '/';	
	}
	iput(root_inode);
	iput(dir_inode);
	current->root = NULL;
	current->pwd = NULL;
}

void from_explorer(char * dir){
	int j = 0, i;	
	populate_explorer(dir);
	for(; j < 10; j++){
		int curr = (j + 1) * 160 + 118;
		int maks = strlen(eValues[j]) >= 20 ? 20 : strlen(eValues[j]);
		short centre = (40 - maks * 2) / 2;

		for(i = 0; i < 40; i += 2){
			if(i >= centre && i <= 40 - centre){
				char chW = eValues[j][(i - centre) / 2];
				short colour = colours[j];
				*(short*)(origin + curr + i) = ((short)colour << 8) | chW;	
			}
			else{
				*(short*)(origin + curr + i) = ((short)0x07 << 8) |0x20;		
			}
		}

	}
}

void go_right(){
	int index = highlighted / 160 - 1, len = 0, i;
	char * path1 = currentDir;

	if(currentDir[1] == 0){
		strcat(path1, eValues[index]);
		from_explorer(path1);
		return;
	}
	len = strlen(path1);
	path1[len] = '/';
	path1[len + 1] = 0;
	len += strlen(eValues[index]) + 1;
	strcat(path1, eValues[index]);	
	path1[len] = 0; 
	from_explorer(path1);
	if(path1[strlen(path1) - 1] == '/')
		path1[strlen(path1) - 1] = 0;

	mark(highlighted, 1);
}

void go_left(){
	int index = highlighted / 160 - 1;
	char * path1 = currentDir;
	int i = strlen(path1) - 1;
	for(; i >= 0; i--){
		if(path1[i] == '/')
			break;	
		path1[i] = 0;
	}
	path1[i] = 0;
	if(path1[0] == 0)
		path1[0] = '/';

	from_explorer(path1);
	mark(highlighted, 1);
}

/*
 *  void con_init(void);
 *
 * This routine initalizes console interrupts, and does nothing
 * else. If you want the screen to clear, call tty_write with
 * the appropriate escape-sequece.
 */
void con_init(void)
{
	register unsigned char a;

	gotoxy(*(unsigned char *)(0x90000+510),*(unsigned char *)(0x90000+511));
	set_trap_gate(0x21,&keyboard_interrupt);
	outb_p(inb_p(0x21)&0xfd,0x21);
	a=inb_p(0x61);
	outb_p(a|0x80,0x61);
	outb(a,0x61);
}

int sys_clear(void)
{
	int i;
	for(i = 0; i < LINES; ++i)
	{
		scrup();
	}
	gotoxy(0,0);
	return 0;
}
