#include <errno.h>
#include <crypt.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>

#define GLOBAL 1000
#define LOCAL 500

int sys_ftime()
{
	return -ENOSYS;
}

int sys_mknod()
{
	return -ENOSYS;
}

int sys_break()
{
	return -ENOSYS;
}

int sys_mount()
{
	return -ENOSYS;
}

int sys_umount()
{
	return -ENOSYS;
}

int sys_ustat(int dev,struct ustat * ubuf)
{
	return -1;
}

int sys_ptrace()
{
	return -ENOSYS;
}

int sys_stty()
{
	return -ENOSYS;
}

int sys_gtty()
{
	return -ENOSYS;
}

int sys_rename()
{
	return -ENOSYS;
}

int sys_prof()
{
	return -ENOSYS;
}

int sys_setgid(int gid)
{
	if (current->euid && current->uid)
		if (current->gid==gid || current->sgid==gid)
			current->egid=gid;
		else
			return -EPERM;
	else
		current->gid=current->egid=gid;
	return 0;
}

int sys_acct()
{
	return -ENOSYS;
}

int sys_phys()
{
	return -ENOSYS;
}

int sys_lock()
{
	return -ENOSYS;
}

int sys_mpx()
{
	return -ENOSYS;
}

int sys_ulimit()
{
	return -ENOSYS;
}

int sys_time(long * tloc)
{
	int i;

	i = CURRENT_TIME;
	if (tloc) {
		verify_area(tloc,4);
		put_fs_long(i,(unsigned long *)tloc);
	}
	return i;
}

int sys_setuid(int uid)
{
	if (current->euid && current->uid)
		if (uid==current->uid || current->suid==current->uid)
			current->euid=uid;
		else
			return -EPERM;
	else
		current->euid=current->uid=uid;
	return 0;
}

int sys_stime(long * tptr)
{
	if (current->euid && current->uid)
		return -1;
	startup_time = get_fs_long((unsigned long *)tptr) - jiffies/HZ;
	return 0;
}

int sys_times(struct tms * tbuf)
{
	if (!tbuf)
		return jiffies;
	verify_area(tbuf,sizeof *tbuf);
	put_fs_long(current->utime,(unsigned long *)&tbuf->tms_utime);
	put_fs_long(current->stime,(unsigned long *)&tbuf->tms_stime);
	put_fs_long(current->cutime,(unsigned long *)&tbuf->tms_cutime);
	put_fs_long(current->cstime,(unsigned long *)&tbuf->tms_cstime);
	return jiffies;
}

int sys_brk(unsigned long end_data_seg)
{
	if (end_data_seg >= current->end_code &&
	    end_data_seg < current->start_stack - 16384)
		current->brk = end_data_seg;
	return current->brk;
}

/*
 * This needs some heave checking ...
 * I just haven't get the stomach for it. I also don't fully
 * understand sessions/pgrp etc. Let somebody who does explain it.
 */
int sys_setpgid(int pid, int pgid)
{
	int i;

	if (!pid)
		pid = current->pid;
	if (!pgid)
		pgid = pid;
	for (i=0 ; i<NR_TASKS ; i++)
		if (task[i] && task[i]->pid==pid) {
			if (task[i]->leader)
				return -EPERM;
			if (task[i]->session != current->session)
				return -EPERM;
			task[i]->pgrp = pgid;
			return 0;
		}
	return -ESRCH;
}

int sys_getpgrp(void)
{
	return current->pgrp;
}

int sys_setsid(void)
{
	if (current->uid && current->euid)
		return -EPERM;
	if (current->leader)
		return -EPERM;
	current->leader = 1;
	current->session = current->pgrp = current->pid;
	current->tty = -1;
	return current->pgrp;
}

int sys_oldolduname(void* v)
{
	printk("calling obsolete system call oldolduname\n");
	return -ENOSYS;
//	return (0);
}

int sys_uname(struct utsname * name)
{
	static struct utsname thisname = {
		"linux 0.01-3.x","nodename","release ","3.x","i386"
	};
	int i;

	if (!name) return -1;
	verify_area(name,sizeof *name);
	for(i=0;i<sizeof *name;i++)
		put_fs_byte(((char *) &thisname)[i],i+(char *) name);
	return (0);
}

int sys_umask(int mask)
{
	int old = current->umask;

	current->umask = mask & 0777;
	return (old);
}

int sys_null(int nr)
{
	static int prev_nr=-2;
	if (nr==174 || nr==175) return -ENOSYS;

	if (prev_nr!=nr) 
	{
		prev_nr=nr;
//		printk("system call num %d not available\n",nr);
	}
	return -ENOSYS;
}
//proj

int r;

int random(int mask){
	__asm__ __volatile__ (
		"rdtsc;"
		: "=a" (r)
		:
		: "%edx"
	);
	if(r < 0){
        r *= -1;
	}
	if(r % mask < 32){
        r += 32;
	}
	return r % mask;
}



int sys_keyclear(int scope){
	int i = 0, len;
	if(scope == 0){
		len = strlen(globalKey);
		for(i = 0; i < len; i++)
			globalKey[i] = 0;
		globalTime = 0;
	}else if(scope == 1){
		len = strlen(current->key);
		for(i = 0; i < len; i++)
			current->key[i] = 0;
		current->keyTime = -1;
	}
	for(i = 0; i < 1024; i++){
		kljuc[i] = 0;	
		pomocniKljuc[i] = 0;
		permutacija[i] = 0;
	}
	return 0;
}

int sys_keyset(const char *key, int scope){
	if(scope == 1){
		int i = 0, len = strlen(key);
		for(i = 0; get_fs_byte(key + i) != 0; i++){
			globalKey[i] = get_fs_byte(key + i);
		}
		globalKey[i] = 0;
		sortKey(NULL);
		i = 0;
	}else if(scope == 0){
		int i = 0, len = strlen(key);
		for(i = 0; get_fs_byte(key + i) != 0; i++){
			current->key[i] = get_fs_byte(key + i);
		}
		current->key[i] = 0;
		sortKey(current->key);
		i = 0;
	}
	return 0;
}

int sys_keygen(int level){
	int len = 2 << level, i = 0;
	for(i = 0; i < len; i++){
		kljuc[i] = random(127);
	}
	kljuc[i] = 0;
	sortKey(NULL);
	i = 0;
	while(kljuc[i] != 0){
			PUTCH(kljuc[i], tty_table[0].write_q);
			i++;
	}
	PUTCH('\n', tty_table[0].write_q);
	return 0;
}


int sys_visibi(int mode){
	if (!mode)
		tty_table[0].termios.c_lflag = tty_table[0].termios.c_lflag & ~ECHO;
	else 
		tty_table[0].termios.c_lflag = tty_table[0].termios.c_lflag | ECHO;
}

void timer(){
	if(globalKey[0] != 0)
		globalTime++;
	if(globalTime == GLOBAL){
		globalTime = 0;
		memset(globalKey, 0, strlen(globalKey));
		keyclear(0);
		printk("Global key reset!\n");	
	}
	int i;
	for(i = 1; i < NR_TASKS; i++){	
		if(task[i]->key[0] != 0){
			task[i]->keyTime++;
		}
		if(task[i]->keyTime == LOCAL){
			task[i]->keyTime = 0;
			memset(task[i]->key, 0, strlen(task[i]->key));
			keyclear(1);
			printk("Local key reset on process %d!\n", task[i]->pid);
			return;	
		}	
	}
}

