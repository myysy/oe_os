/* 文件相关函数 */

#include "bootpack.h"

void file_readfat(int *fat, unsigned char *img)
/*将磁盘映像中的FAT解压缩 */
{
	int i, j = 0;
	for (i = 0; i < 2880; i += 2) {
		fat[i + 0] = (img[j + 0]      | img[j + 1] << 8) & 0xfff;
		fat[i + 1] = (img[j + 1] >> 4 | img[j + 2] << 4) & 0xfff;
		j += 3;
	}
	return;
}

void file_loadfile(int clustno, int size, char *buf, int *fat, char *img)
{
	int i;
	for (;;) {
		if (size <= 512) {
			for (i = 0; i < size; i++) {
				buf[i] = img[clustno * 512 + i];
			}
			break;
		}
		for (i = 0; i < 512; i++) {
			buf[i] = img[clustno * 512 + i];
		}
		size -= 512;
		buf += 512;
		clustno = fat[clustno];
	}
	return;
}

struct FILEINFO *file_search(char *name, struct FILEINFO *finfo, int max)
{
	int i, j;
	char s[12];
	for (j = 0; j < 11; j++) {
		s[j] = ' ';
	}
	j = 0;
	for (i = 0; name[i] != 0; i++) {
		if (j >= 11) { return 0; /*没有找到*/ }
		if (name[i] == '.' && j <= 8) {
			j = 8;
		} else {
			s[j] = name[i];
			if ('a' <= s[j] && s[j] <= 'z') {
				/*将小写字母转换为大写字母*/
				s[j] -= 0x20;
			} 
			j++;
		}
	}
	for (i = 0; i < max; ) {
		if (finfo->name[0] == 0x00) {
			break;
		}
		if ((finfo[i].type & 0x18) == 0 || (finfo[i].type & 0x28) == 0) {
			for (j = 0; j < 11; j++) {
				if (finfo[i].name[j] != s[j]) {
					goto next;
				}
			}
			return finfo + i; /*找到文件*/
		}		
next:
		i++;
	}
	return 0; /*没有找到*/
}

char *file_loadfile2(int clustno, int *psize, int *fat)
{
	int size = *psize, size2;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	char *buf, *buf2;
	buf = (char *) memman_alloc_4k(memman, size);
	file_loadfile(clustno, size, buf, fat, (char *) (ADR_DISKIMG + 0x003e00));
	if (size >= 17) {
		size2 = tek_getsize(buf);
		if (size2 > 0) {	/*使用tek格式压缩的文件*/
			buf2 = (char *) memman_alloc_4k(memman, size2);
			tek_decomp(buf, buf2, size2);
			memman_free_4k(memman, (int) buf, size);
			buf = buf2;
			*psize = size2;
		}
	}
	return buf;
}

//返回句柄号
int file_open(char *name, struct FILEINFO *finfo, int max){
	struct FILEINFO *fileinfo = file_search(name, finfo, max);
	if (fileinfo != 0)
	{
		return fileinfo->clustno;
	}
	return 0;
}

//buf内存写入磁盘
void file_loadimg(char *buf, int clustno, int pos, int size){
	char *img = (char *)(ADR_DISKIMG + 0x004200 + (clustno - 2) * 512);
	int i;
	for (i = 0; i < size; i++)
	{
		img[pos + i] = buf[i];
	}
	return;
}

//内容写入文件
void file_write(int fh, char *buf, int size){
	char *img = (char *)(ADR_DISKIMG + 0x004200 + (fh - 2) * 512);
	int i;
	for (i = 0; i < size; i++)
	{
		img[i] = buf[i];
	}
}

//从磁盘上删除文件及其fileinfo
int file_del(struct FILEINFO *finfo, int *fat){
	char *img;
	int i, del_fat;

	//删目录项
	finfo->name[0] = 0xe5;
	int next_clustno = finfo->clustno;
	//删除文件内容
	while(next_clustno != 0xfff){
		img = (char *)(ADR_DISKIMG + 0x004200 + (next_clustno - 2) * 512);
		for (i = 0; i < 512 && (img[i] != 0x00); i++){
			img[i] = 0x00;
		}
		//改变fat表
		del_fat = next_clustno;
		next_clustno = fat[next_clustno];
		fat[del_fat] = 0;
	}
	return 1;
}

int  dir_del(struct FILEINFO *finfo, int *fat){
	int i;	
	// int del_fat;

	//删目录项
	// finfo->name[0] = 0xe5;
	int next_clustno = finfo->clustno;
	// 遍历删除文件内容
	while(next_clustno != 0xfff){
		//问题所在点
		struct FILEINFO *next_finfo = (struct FILEINFO *)(ADR_DISKIMG + 0x004200 + (next_clustno - 2) * 512);
		for (i = 0; i < 16 && next_finfo->name[0] != 0x00; i++){
			if (next_finfo[i].name[0] == 0xe5)
				continue;
			//文件
			if (next_finfo[i].type != 0 && (next_finfo[i].type & 0x18) == 0) {
				file_del(next_finfo + i, fat);
			}			
			//目录
			else if(next_finfo[i].type != 0 && (next_finfo[i].type & 0x28) == 0){
				dir_del(next_finfo + i, fat);
			}
		}
		//改变fat表
		// del_fat = next_clustno;
		next_clustno = fat[next_clustno];
		// fat[del_fat] = 0;
	}
	file_del(finfo, fat);
	return 1;
}

//修改文件属性
void file_chattr(struct FILEINFO *finfo, char sign, char attr){
	if (sign == '+' && ((finfo->type & attr) == 0)){
		finfo->type = finfo->type + attr;
	}
	else if (sign == '-' && ((finfo->type & attr) != 0)){
		finfo->type = finfo->type - attr;
	}	
}
