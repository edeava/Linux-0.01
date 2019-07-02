#include <errno.h>
#include <fcntl.h>
#include <crypt.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

char *bstr = "\0";
int fileNum = 0;
int encrypted[1024] = {0}, keys[1024] = {0};

int file_read(struct m_inode * inode, struct file * filp, char * buf, int count)
{
	int left,chars,nr, encrNeeded = 0, i;
	struct buffer_head * bh;
	for(i = 0; i < 1024; i++){
		if(encrypted[i] == inode->i_num && kljuc[0] != 0){
			if(keys[i] != hash(kljuc)){
				printk("Wrong key!\n");
				return 0;
			}
			file_decr(inode);
			encrNeeded = 1;
		}
	}

	if ((left=count)<=0)
		return 0;
	while (left) {
		if ((nr = bmap(inode,(filp->f_pos)/BLOCK_SIZE))) {
			if (!(bh=bread(inode->i_dev,nr)))
				break;
		} else
			bh = NULL;
		nr = filp->f_pos % BLOCK_SIZE;
		chars = MIN( BLOCK_SIZE-nr , left );
		filp->f_pos += chars;
		left -= chars;
		if (bh) {
			char * p = nr + bh->b_data;
			while (chars-->0)
				put_fs_byte(*(p++),buf++);
			brelse(bh);
		} else {
			while (chars-->0)
				put_fs_byte(0,buf++);
		}
	}
	if(encrNeeded)
		file_encr(inode);
	inode->i_atime = CURRENT_TIME;
	return (count-left)?(count-left):-ERROR;
}



int sys_encr(int fd){
	
	struct file * file;
	struct m_inode * inode;
	int i = 0;
	keyclear(10);
	if(current->key[0] != 0)
		sortKey(current->key);
	else if(globalKey[0] != 0)
		sortKey(NULL);
	
	if(fd >= NR_OPEN || !(file = current->filp[fd]) || (kljuc[0] == 0))
		return -EINVAL;
	inode = file->f_inode;

	file_encr(inode);
	encrypted[fileNum] = inode->i_num;
	keys[fileNum] = hash(kljuc);
	copyTo();
	
	fileNum++;
	char buffer[30] = "\0";
	i = 0;
	printk("%s\n", bstr);
	return 0;
}

void copyFrom(){
	char buf[1024] = "\0";
	int k = 0, j = 0, i = 0;
	while(bstr[i] != 0){
		if(bstr[i] == '~'){
			i++;
			j = 0;
			if(buf[0] != '\0'){
				encrypted[k] = atoi(buf);
				k++;
			}
			buf[0] = '\0';
			continue;
		}else if(bstr[i] == '|'){
			i++;
			j = 0;
			if(buf[0] != '\0'){
				keys[k - 1] = atoi(buf);
			}
			buf[0] = '\0';
		}
        buf[j + 1] = 0;
		buf[j] = bstr[i];
		j++;
		i++;
	}
	fileNum = k;
}

void copyTo(){
	char buf[1024] = "\0";
	int k = 0, j = 0, i = 0;
	strcpy(bstr, "|");
	for(i = 0; i < 1024; i++){
		if(encrypted[i] == 0) continue;
		itoa(encrypted[i], buf);
		strcat(bstr, buf);
		strcat(bstr, "~");
		buf[0] = '\0';
		itoa(keys[i], buf);
		strcat(bstr, buf);
		strcat(bstr, "|");
		buf[0] = '\0';
	}
}

int sys_decr(int fd){

	struct file * file;
	struct m_inode * inode;
	keyclear(10);
	if(current->key[0] != 0)
		sortKey(current->key);
	else if(globalKey[0] != 0)
		sortKey(NULL);
	
	if(fd >= NR_OPEN || !(file = current->filp[fd]) || (kljuc[0] == 0))
		return -EINVAL;
	inode = file->f_inode;
	int i;	
	for(i = 0; i < 1024; i++){
		if(encrypted[i] == inode->i_num){
			if(hash(kljuc) != keys[i])
				return -EINVAL;
		}
	}
	file_decr(inode);
	for(i = 0; i < 1024; i++){
		if(encrypted[i] == inode->i_num && keys[i] == hash(kljuc)){
			encrypted[i] = 0;
			keys[i] = 0;
		}
	}
	copyTo();
	return 0;
}

int file_encr(struct m_inode * inode){

	int nr, i = 0;
	//
	struct buffer_head *buff_head;
	struct dir_entry *file;
	if(S_ISDIR(inode->i_mode)){	
		int len = 1, j;
		struct m_inode * child_inode;

		buff_head = bread(0x301,inode->i_zone[0]);
		file = (struct dir_entry*) buff_head->b_data;
		file += 2;
		while (file->name[0]){
			child_inode = iget(0x301, file->inode);

			file_encr(child_inode);
			encrypted[fileNum] = child_inode->i_num;
			keys[fileNum] = hash(kljuc);
			copyTo();
			fileNum++;

			iput(child_inode);
			file++;
		}
		do{
			buff_head = bread(inode->i_dev, nr);
			if(!buff_head) break;
			encrypt(buff_head->b_data);
			buff_head->b_dirt = 1;
			brelse(buff_head);
			i++;
		}while(nr = bmap(inode, i));
	}else{	
	//
		struct buffer_head *bh;
		while(nr = bmap(inode, i)){
			bh = bread(inode->i_dev, nr);
			if(!bh) break;
			encrypt(bh->b_data);
			bh->b_dirt = 1;
			brelse(bh);
			i++;
		}
	}
	return 0;	 	
}

int blockInit(){
	int b_num, i = 0;
	struct buffer_head *bh;
	struct m_inode * inode;
	inode = iget(0x301, 1);
	b_num = 9836;
	bh = bread(inode->i_dev, b_num);
	bh->b_dirt = 1;
	bstr = bh->b_data;
	i = 0;
	copyFrom();
	brelse(bh);

	return b_num;
}

int file_decr(struct m_inode * inode){

	int nr, i = 0;
	struct buffer_head *bh;
	while(nr = bmap(inode, i)){
		bh = bread(inode->i_dev, nr);
		if(!bh) break;
		decrypt(bh->b_data);
		bh->b_dirt = 1;
		brelse(bh);
		i++;
	}

	struct buffer_head *buff_head;
	struct dir_entry *file;
	if(S_ISDIR(inode->i_mode)){	
		int len = 1, j;
		struct m_inode * child_inode;

		buff_head = bread(0x301,inode->i_zone[0]);
		file = (struct dir_entry*) buff_head->b_data;
		file += 2;
		while (file->name[0]){
			child_inode = iget(0x301, file->inode);

			file_decr(child_inode);
			for(i = 0; i < 1024; i++){
				if(encrypted[i] == child_inode->i_num && keys[i] == hash(kljuc)){
					encrypted[i] = 0;
					keys[i] = 0;
				}
			}
			copyTo();

			iput(child_inode);
			file++;
		}
	}	
	
	return 0;	
}

int file_write(struct m_inode * inode, struct file * filp, char * buf, int count)
{
	off_t pos;
	int block,c;
	struct buffer_head * bh;
	char * p;
	int i=0, decrNeeded = 0;

	for(i = 0; i < 1024; i++){
		if(encrypted[i] == inode->i_num && kljuc[0] != 0){
			if(keys[i] != hash(kljuc)){
				printk("Wrong key!\n");
				return 0;
			}
			file_encr(inode);
			decrNeeded = 1;
		}
	}

/*
 * ok, append may not work when many processes are writing at the same time
 * but so what. That way leads to madness anyway.
 */
	if (filp->f_flags & O_APPEND)
		pos = inode->i_size;
	else
		pos = filp->f_pos;
	while (i<count) {
		if (!(block = create_block(inode,pos/BLOCK_SIZE)))
			break;
		if (!(bh=bread(inode->i_dev,block)))
			break;
		c = pos % BLOCK_SIZE;
		p = c + bh->b_data;
		bh->b_dirt = 1;
		c = BLOCK_SIZE-c;
		if (c > count-i) c = count-i;
		pos += c;
		if (pos > inode->i_size) {
			inode->i_size = pos;
			inode->i_dirt = 1;
		}
		i += c;
		while (c-->0)
			*(p++) = get_fs_byte(buf++);
		brelse(bh);
	}
	if(decrNeeded)
		file_decr(inode);
	inode->i_mtime = CURRENT_TIME;
	if (!(filp->f_flags & O_APPEND)) {
		filp->f_pos = pos;
		inode->i_ctime = CURRENT_TIME;
	}
	return (i?i:-1);
}
