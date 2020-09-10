/* 命令行窗口相关 */

#include "bootpack.h"
#include <stdio.h>
#include <string.h>

//////////////////////////////////////
struct FILEINFO *current_dir = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
int current_clustno = -12;
int vi_flag = 0;
int vi_pos;
struct FILEINFO *edit_file;

void console_task(struct SHEET *sheet, int memtotal)
{
	struct TASK *task = task_now();
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	int i, *fat = (int *) memman_alloc_4k(memman, 4 * 2880);
	struct FILEHANDLE fhandle[8];
	struct CONSOLE cons;
	char cmdline[30];
	unsigned char *nihongo = (char *) *((int *) 0x0fe8);

	////////////////////////////////////////////////////////////////
	for (i = 0; i < 4;i++){
		current_dir->reserve[i] = (current_clustno >> (8 * (3 - i))) & 0xff;
	}
	//原文件 -> 系统文件
	for (i = 0; i < 224 && current_dir[i].name[0] != 0x00;i++){
		current_dir[i].type += 0x04;
	}
	////////////////////////////////////////////////////////////////

	cons.sht = sheet;
	cons.cur_x =  8;
	cons.cur_y = 28;
	cons.cur_c = -1;
	task->cons = &cons;
	task->cmdline = cmdline;

	if (cons.sht != 0) {
		cons.timer = timer_alloc();
		timer_init(cons.timer, &task->fifo, 1);
		timer_settime(cons.timer, 50);
	}
	file_readfat(fat, (unsigned char *)(ADR_DISKIMG + 0x000200));
	for (i = 0; i < 8; i++) {
		fhandle[i].buf = 0; /*未使用标记*/
	}
	task->fhandle = fhandle;
	task->fat = fat;
	if (nihongo[4096] != 0xff) {	 /* 是否载入了字库？*/
		task->langmode = 1;
	} else {
		task->langmode = 0;
	}
	task->langbyte1 = 0;

	/*显示提示符*/
	cons_putchar(&cons, '>', 1);

	for (;;) {
		io_cli();
		if (fifo32_status(&task->fifo) == 0) {
			task_sleep(task);
			io_sti();
		} else {
			i = fifo32_get(&task->fifo);
			io_sti();
			if (i <= 1 && cons.sht != 0) { /*光标用定时器*/
				if (i != 0) {
					timer_init(cons.timer, &task->fifo, 0); /*下次置0 */
					if (cons.cur_c >= 0) {
						cons.cur_c = COL8_FFFFFF;
					}
				} else {
					timer_init(cons.timer, &task->fifo, 1); /*下次置1 */
					if (cons.cur_c >= 0) {
						cons.cur_c = COL8_000000;
					}
				}
				timer_settime(cons.timer, 50);
			}
			if (i == 2) { /*光标ON */
				cons.cur_c = COL8_FFFFFF;
			}
			if (i == 3) { /*光标OFF */
				if (cons.sht != 0) {
					boxfill8(cons.sht->buf, cons.sht->bxsize, COL8_000000,
						cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
				}
				cons.cur_c = -1;
			}
			if (i == 4) { /*点击命令行窗口的“×”按钮*/ 
				cmd_exit(&cons, fat);
			}
			if (256 <= i && i <= 511) { /*键盘数据（通过任务A）*/
				if (i == 8 + 256) {
					/*退格键*/
					if (cons.cur_x > 16) {
					/*用空格擦除光标后将光标前移一位*/
						cons_putchar(&cons, ' ', 0);
						cons.cur_x -= 8;
					}
				} else if (i == 10 + 256) {
					/*回车键*/
					/*将光标用空格擦除后换行 */
					cons_putchar(&cons, ' ', 0);
					cmdline[cons.cur_x / 8 - 2] = 0;
					cons_newline(&cons);
					cons_runcmd(cmdline, &cons, fat, memtotal); /*运行命令*/
					if (cons.sht == 0) {
						cmd_exit(&cons, fat);
					}
					/*显示提示符*/
					cons_putchar(&cons, '>', 1);
				} else {
					/*一般字符*/
					if (cons.cur_x < 240) {
						/*显示一个字符之后将光标后移一位*/
						cmdline[cons.cur_x / 8 - 2] = i - 256;
						cons_putchar(&cons, i - 256, 1);
					}
				}
			}
			/*重新显示光标*/
			if (cons.sht != 0) {
				if (cons.cur_c >= 0) {
					boxfill8(cons.sht->buf, cons.sht->bxsize, cons.cur_c, 
						cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
				}
				sheet_refresh(cons.sht, cons.cur_x, cons.cur_y, cons.cur_x + 8, cons.cur_y + 16);
			}
		}
	}
}

void cons_putchar(struct CONSOLE *cons, int chr, char move)
{
	char s[2];
	s[0] = chr;
	s[1] = 0;
	if (s[0] == 0x09) { /*制表符*/
		for (;;) {
			if (cons->sht != 0) {
				putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
			}
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240) {
				cons_newline(cons);
			}
			if (((cons->cur_x - 8) & 0x1f) == 0) {
				break; /*被32整除则break*/
			}
		}
	} else if (s[0] == 0x0a) { /*换行*/
		cons_newline(cons);
	} else if (s[0] == 0x0d) { /*回车*/
		/*先不做任何操作*/
	} else { /*一般字符*/
		if (cons->sht != 0) {
			putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
		}
		if (move != 0) {
			/* move为0时光标不后移*/
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240) {
				cons_newline(cons);
			}
		}
	}
	return;
}

void cons_newline(struct CONSOLE *cons)
{
	int x, y;
	struct SHEET *sheet = cons->sht;
	struct TASK *task = task_now();
	if (cons->cur_y < 28 + 112) {
		cons->cur_y += 16; /*到下一行*/
	} else {
		/*滚动*/
		if (sheet != 0) {
			for (y = 28; y < 28 + 112; y++) {
				for (x = 8; x < 8 + 240; x++) {
					sheet->buf[x + y * sheet->bxsize] = sheet->buf[x + (y + 16) * sheet->bxsize];
				}
			}
			for (y = 28 + 112; y < 28 + 128; y++) {
				for (x = 8; x < 8 + 240; x++) {
					sheet->buf[x + y * sheet->bxsize] = COL8_000000;
				}
			}
			sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
		}
	}
	cons->cur_x = 8;
	if (task->langmode == 1 && task->langbyte1 != 0) {
		cons->cur_x += 8;
	}
	return;
}

void cons_putstr0(struct CONSOLE *cons, char *s)
{
	for (; *s != 0; s++) {
		cons_putchar(cons, *s, 1);
	}
	return;
}

void cons_putstr1(struct CONSOLE *cons, char *s, int l)
{
	int i;
	for (i = 0; i < l; i++) {
		cons_putchar(cons, s[i], 1);
	}
	return;
}

void cons_runcmd(char *cmdline, struct CONSOLE *cons, int *fat, int memtotal)
{
	if (strcmp(cmdline, "mem") == 0 && cons->sht != 0) {
		cmd_mem(cons, memtotal);
	} else if (strcmp(cmdline, "cls") == 0 && cons->sht != 0) {
		cmd_cls(cons);
	} else if ((strcmp(cmdline, "dir") == 0 || strcmp(cmdline, "ls") == 0) && cons->sht != 0) {
		cmd_dir(cons);
	} else if (strncmp(cmdline, "mkdir ", 6) == 0) {
		cmd_mkdir(cons, fat, cmdline);	
	} else if (strncmp(cmdline, "touch ", 6) == 0) {
		cmd_touch(cons, fat, cmdline);	
	} else if (strncmp(cmdline, "vi ", 3) == 0) {
		cmd_vi(cons, fat, cmdline);
	} else if (vi_flag == 1 && cons->sht != 0) {
		cmd_edit(cons, cmdline);
	} else if (strncmp(cmdline, "del ", 4) == 0) {
		cmd_del(cons, fat, cmdline);
	} else if (strncmp(cmdline, "rd ", 3) == 0) {
		cmd_rd(cons, fat, cmdline);
	} else if (strcmp(cmdline, "cd") == 0 && cons->sht != 0) {
		cmd_cdroot();
	} else if (strcmp(cmdline, "cd .") == 0 && cons->sht != 0) {
		cmd_cdparent();
	} else if (strncmp(cmdline, "cd ", 3) == 0) {
		cmd_cd(cons, fat, cmdline);
	} else if ((strncmp(cmdline, "attr +", 6) == 0 || strncmp(cmdline, "attr -", 6) == 0) && cmdline[7]==' ') {
		cmd_attr(cons, fat, cmdline);
	} else if (strcmp(cmdline, "lsattr") == 0 && cons->sht != 0) {
		cmd_lsattr(cons);
	} else if (strcmp(cmdline, "tree") == 0 && cons->sht != 0) {
		cmd_tree(cons, current_clustno, 0);
		cons_putchar(cons, '\n', 1);
	} else if (strcmp(cmdline, "path") == 0 && cons->sht != 0) {
		char s[30] = "";
		cmd_path(cons, current_dir[0], s);
		cons_putstr0(cons, s);
		cons_putchar(cons, '\n', 1);
		cons_putchar(cons, '\n', 1);
	} else if (strcmp(cmdline, "exit") == 0) {
		cmd_exit(cons, fat);
	} else if (strncmp(cmdline, "start ", 6) == 0) {
		cmd_start(cons, cmdline, memtotal);
	} else if (strncmp(cmdline, "ncst ", 5) == 0) {
		cmd_ncst(cons, cmdline, memtotal);
	} else if (strncmp(cmdline, "langmode ", 9) == 0) {
		cmd_langmode(cons, cmdline);
	}else if (cmdline[0] != 0) {
		if (cmd_app(cons, fat, cmdline) == 0) {
			/*不是命令，不是应用程序，也不是空行*/
			cons_putstr0(cons, "Bad command.\n\n");
		}
	}
	return;
}

void cmd_mem(struct CONSOLE *cons, int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	char s[60];
	sprintf(s, "total   %dMB\nfree %dKB\n\n", memtotal / (1024 * 1024), memman_total(memman) / 1024);
	cons_putstr0(cons, s);
	return;
}

void cmd_cls(struct CONSOLE *cons)
{
	int x, y;
	struct SHEET *sheet = cons->sht;
	for (y = 28; y < 28 + 128; y++) {
		for (x = 8; x < 8 + 240; x++) {
			sheet->buf[x + y * sheet->bxsize] = COL8_000000;
		}
	}
	sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
	cons->cur_y = 28;
	return;
}

////////////////////////////////////////////////////////////////////////////////
void cmd_dir(struct CONSOLE *cons)
{
	//struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	struct FILEINFO *finfo = current_dir;
	int i, j;
	char s[30];
	
	int max_index = 16;
	if(current_clustno <= 0){
		max_index = 224;
	}
	for (i = 0; i < max_index; i++) {
		if (finfo[i].name[0] == 0x00) {
			break;
		}
		if (finfo[i].name[0] != 0xe5)
		{
			//文件
			if ((finfo[i].type & 0x18) == 0) {
				//系统文件或隐藏文件不显示
			 	// if (finfo[i].type & 0x04) == 0 && (finfo[i].type & 0x02) == 0){
				if ((finfo[i].type & 0x06) == 0) {	 
					sprintf(s, "filename.ext   %7d\n", finfo[i].size);
					for (j = 0; j < 8; j++) {
						s[j] = finfo[i].name[j];
					}
					s[ 9] = finfo[i].ext[0];
					s[10] = finfo[i].ext[1];
					s[11] = finfo[i].ext[2];
					cons_putstr0(cons, s);
				}
			}
			//目录
			else if((finfo[i].type & 0x28) == 0){
				sprintf(s, "filename DIR          \n");
				for (j = 0; j < 8; j++) {
					s[j] = finfo[i].name[j];
				}
				cons_putstr0(cons, s);
			}
		}
	}
	cons_newline(cons);
	return;
}

void cmd_mkdir(struct CONSOLE *cons, int *fat, char *cmdline)
{
	//struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	struct FILEINFO *finfo = current_dir;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct FILEINFO *new_dir = (struct FILEINFO *) memman_alloc(memman, sizeof(struct FILEINFO));
	int i, j;
	char s[30];
	char name[12] = "";

	//拿到新文件夹名
	for (j = 0; j < 8;j++){
		new_dir->name[j] = ' ';
	}
	for (i = 6, j = 0; cmdline[i] != 0 && j < 8; i++, j++)
	{
		if('a' <= cmdline[i] && cmdline[i] <= 'z'){
			cmdline[i] -= 0x20;
		}
		new_dir->name[j] = cmdline[i];
		name[j] = cmdline[i];
	}
	name[j] = '.';
	new_dir->ext[0] = name[j+1] = 'D';
	new_dir->ext[1] = name[j+2] = 'I';
	new_dir->ext[2] = name[j+3] = 'R';
	new_dir->type = 0x10;

	//是否文件夹已存在
	int max_index = 16;
	if(current_clustno <= 0){
		max_index = 224;
	}
	struct FILEINFO *is_exsit = file_search(name, finfo, max_index);
	//存在
	if(is_exsit != 0){
		cons_putstr0(cons, "The directory exists.\n\n");
	}
	else{
		//前4位父目录所在簇号，后4位父目录所在位置pos(0~15/223)
		for (i = 0; i < 8;i++){
			new_dir->reserve[i] = current_dir->reserve[i];
		}	
		//寻找新的空间
		for (i = 0; i < max_index;i++){
			if(finfo[i].name[0] == 0xe5 || finfo[i].name[0] == 0x00){
				break;
			}
		}
		if (i < max_index){
			//fat表更新
			for (j = 2; j < 2280;j++){
				if (fat[j] == 0){
					new_dir->clustno = j;
					fat[j] = 0xfff;
					break;
				}
			}
			//目录项写入磁盘
			file_loadimg((char *)new_dir, current_clustno, i * 32, 32);

			//打印文件夹名字
			sprintf(s, "filename DIR          \n");
			for (j = 0; j < 8; j++) {
				s[j] = new_dir->name[j];
			}		
			cons_putstr0(cons, s);
			cons_newline(cons);
		}
		else{
			cons_putstr0(cons, "Space is not enough.\n\n");
		}
	}	
	//释放申请的内存
	memman_free(memman, (int)new_dir, sizeof(struct FILEINFO));
	return;
}

void cmd_touch(struct CONSOLE *cons, int *fat, char *cmdline)
{
	//struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	struct FILEINFO *finfo = current_dir;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct FILEINFO *new_file = (struct FILEINFO *) memman_alloc(memman, sizeof(struct FILEINFO));
	int i, j;
	char s[30];	

	//拿到新文件名
	for (j = 0; j < 8;j++){
		new_file->name[j] = ' ';
	}
	for (i = 6, j = 0; cmdline[i] != '.' && cmdline[i] != 0 && j < 8; i++, j++)
	{
		new_file->name[j] = cmdline[i];
		if('a' <= cmdline[i] && cmdline[i] <= 'z'){
			cmdline[i] -= 0x20;
		}
		new_file->name[j] = cmdline[i];
	}
	//扩展名
	i += 1;
	for (j = 0; cmdline[i] != 0 && j < 3; i++, j++)
	{
		if('a' <= cmdline[i] && cmdline[i] <= 'z'){
			cmdline[i] -= 0x20;
		}
		new_file->ext[j] = cmdline[i];
	}
	new_file->type = 0x20;
	new_file->size = 0;

	int max_index = 16;
	if(current_clustno < 0){
		max_index = 224;
	}
	struct FILEINFO *is_exsit = file_search(cmdline + 6, finfo, max_index);
	//存在
	if(is_exsit != 0){	
		cons_putstr0(cons, "The file exists.\n\n");	
	}
	else{	
		//前4位父目录所在簇号，后4位父目录所在位置pos(0~15/223)
		for (i = 0; i < 8;i++){
			new_file->reserve[i] = current_dir->reserve[i];
		}	

		//寻找新的空间
		for (i = 0; i < max_index;i++){
			if(finfo[i].name[0] == 0xe5 || finfo[i].name[0] == 0x00){
				break;
			}
		}
		if (i < max_index){
			//fat表更新
			for (j = 2; j < 2280;j++){
				if (fat[j] == 0){
					new_file->clustno = j;
					fat[j] = 0xfff;
					break;
				}
			}		
			//写入磁盘
			file_loadimg((char *)new_file, current_clustno, i * 32, 32);

			//打印文件名字与大小			
			sprintf(s, "filename.ext   %7d\n", new_file->size);
			for (j = 0; j < 8; j++) {
				s[j] = new_file->name[j];
			}
			s[ 9] = new_file->ext[0];
			s[10] = new_file->ext[1];
			s[11] = new_file->ext[2];
			cons_putstr0(cons, s);
			cons_newline(cons);
		}
		else{
			cons_putstr0(cons, "Space is not enough.\n\n");
		}
	}
	//释放申请的内存
	memman_free(memman, (int)new_file, sizeof(struct FILEINFO));
	return;
}

void cmd_vi(struct CONSOLE *cons, int *fat, char *cmdline)
{
	struct FILEINFO *finfo = current_dir;
	int i, j;

	for (i = 3; cmdline[i] != 0; i++){
		if('a' <= cmdline[i] && cmdline[i] <= 'z'){
			cmdline[i] -= 0x20;
		}
	}

	int max_index = 16;
	if(current_clustno < 0){
		max_index = 224;
	}
	//寻找文件
	int fh = file_open(cmdline + 3, finfo, max_index);
	if(fh == 0){//不存在
		cons_putstr0(cons, "Please input content.\n");
		struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
		struct FILEINFO *vi_file = (struct FILEINFO *) memman_alloc(memman, sizeof(struct FILEINFO));

		//拿到文件名
		for (j = 0; j < 8;j++){
			vi_file->name[j] = ' ';
		}
		for (i = 3, j = 0; cmdline[i] != '.' && cmdline[i] != 0 && j < 8; i++, j++)
		{
			vi_file->name[j] = cmdline[i];
		}
		//扩展名
		i += 1;
		for (j = 0; cmdline[i] != 0 && j < 3; i++, j++)
		{
			vi_file->ext[j] = cmdline[i];
		}	
		vi_file->type = 0x20;

		//前4位父目录所在簇号，后4位父目录所在位置pos(0~15/223)
		for (i = 0; i < 8;i++){
			vi_file->reserve[i] = current_dir->reserve[i];
		}	
	
		for (i = 0; i < max_index;i++){
			if(finfo[i].name[0] == 0xe5 || finfo[i].name[0] == 0x00){
				break;
			}
		}
		if (i < max_index){
			//fat表更新
			for (j = 2; j < 2280;j++){
				if (fat[j] == 0){
					vi_file->clustno = j;
					fat[j] = 0xfff;
					break;
				}
			}
			edit_file = vi_file;
			vi_pos = i * 32;			
			vi_flag = 1;
		}
		else {
			cons_putstr0(cons, "Space is not enough.\n\n");
		}
	}
	else{//存在
		struct FILEINFO *is_exsit = file_search(cmdline + 3, finfo, max_index);
		if (is_exsit->type != 0 && ((is_exsit->type & 0x18) == 0))
		{
			if((is_exsit->type & 0x01) != 0){
				cons_putstr0(cons, "The file is read-only.\n\n");
			}
			else if((is_exsit->type & 0x04) != 0){
				cons_putstr0(cons, "The file is system-file.\n\n");
			}
			else{
				cons_putstr0(cons, "The file exists.\n");
			}
		}
	}
	return;
}

void cmd_edit(struct CONSOLE *cons, char *cmdline){
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	//文件目录项
	edit_file->size = (unsigned int)strlen(cmdline);
	file_loadimg((char *)edit_file, current_clustno, vi_pos, 32);
	//文件内容
	int max_index = 16;
	if(current_clustno < 0){
		max_index = 224;
	}

	char name[12] = "";
	int i;
	for (i = 0; edit_file->name[i] != ' ' && i < 8;i++){
		name[i] = edit_file->name[i];
	}
	name[i] = '.';
	name[i+1] = edit_file->ext[0];
	name[i+2] = edit_file->ext[1];
	name[i+3] = edit_file->ext[2];

	int fh = file_open(name, current_dir, max_index);
	if (fh != 0)
	{
		file_write(fh, cmdline, edit_file->size);
	}
	vi_flag = 0;
	//释放申请的内存
	memman_free(memman, (int)edit_file, sizeof(struct FILEINFO));
	return;
}

void cmd_del(struct CONSOLE *cons, int *fat, char *cmdline)
{
	//struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	struct FILEINFO *finfo = current_dir;
	int i;

	for (i = 4; cmdline[i] != 0; i++){
		if('a' <= cmdline[i] && cmdline[i] <= 'z'){
			cmdline[i] -= 0x20;
		}
	}

	int max_index = 16;
	if(current_clustno < 0){
		max_index = 224;
	}
	//寻找文件
	struct FILEINFO *is_exsit = file_search(cmdline + 4, finfo, max_index);
	//存在
	if(is_exsit != 0 && is_exsit->type != 0 && ((is_exsit->type & 0x18) == 0)){
		if ((is_exsit->type & 0x04) != 0){
			cons_putstr0(cons, "File can not be deleted.\n\n");
		}
		else{
			file_del(is_exsit, fat);
		}	
			
	}
	else{
		cons_putstr0(cons, "File does not exist.\n\n");
	}
	return;
}

void cmd_rd(struct CONSOLE *cons, int *fat, char *cmdline)
{
	//struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	struct FILEINFO *finfo = current_dir;
	int i, j;
	char name[12] = "";

	//拿到文件夹名
	for (i = 3, j = 0; cmdline[i] != 0 && j < 8; i++, j++)
	{
		if('a' <= cmdline[i] && cmdline[i] <= 'z'){
			cmdline[i] -= 0x20;
		}
		name[j] = cmdline[i];
	}
	name[j] = '.';
	name[j+1] = 'D';
	name[j+2] = 'I';
	name[j+3] = 'R';

	//是否文件已存在
	int max_index = 16;
	if(current_clustno < 0){
		max_index = 224;
	}
	struct FILEINFO *is_exsit = file_search(name, finfo, max_index);
	//存在
	if( is_exsit != 0 ){
		dir_del(is_exsit, fat);
	}
	else{
		cons_putstr0(cons, "Directory does not exist.\n\n");
	}
	return;
}

void cmd_cdroot(){
	current_dir = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	current_clustno = -12;
}

void cmd_cdparent(){
	current_clustno = (current_dir->reserve[0] << 24) | (current_dir->reserve[1] << 16) | (current_dir->reserve[2] << 8) | current_dir->reserve[3];
	// int pos = (current_dir->reserve[4] << 24) | (current_dir->reserve[5] << 16) | (current_dir->reserve[7] << 8) | current_dir->reserve[7];
	current_dir = (struct FILEINFO *)(ADR_DISKIMG + 0x004200 + (current_clustno - 2) * 512);
}

void cmd_cd(struct CONSOLE *cons, int *fat, char *cmdline)
{
	//struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	struct FILEINFO *finfo = current_dir;
	int i, j;
	char name[12] = "";

	//拿到文件夹名
	for (i = 3, j = 0; cmdline[i] != 0 && j < 8; i++, j++)
	{
		if('a' <= cmdline[i] && cmdline[i] <= 'z'){
			cmdline[i] -= 0x20;
		}
		name[j] = cmdline[i];
	}
	name[j] = '.';
	name[j+1] = 'D';
	name[j+2] = 'I';
	name[j+3] = 'R';

	//是否文件夹已存在
	int max_index = 16;
	if(current_clustno < 0){
		max_index = 224;
	}
	struct FILEINFO *is_exsit = file_search(name, finfo, max_index);
	//存在
	if(is_exsit != 0){
		current_dir = (struct FILEINFO *) (ADR_DISKIMG + 0x004200 + (is_exsit->clustno - 2) * 512);
		for (i = 0; i < 4;i++){
			current_dir->reserve[i] = (current_clustno >> (8 * (3 - i))) & 0xff;
		}
		//找到父文件夹位置
		for (i = 0; i < max_index;i++){
			if(finfo + i == is_exsit){
				break;
			}
		}
		for (j = 4; j < 8; j++){
			current_dir->reserve[j] = (i >> (8 * (7 - j))) & 0xff;
		}
		current_clustno = is_exsit->clustno;		
	}
	else{
		cons_putstr0(cons, "Directory does not exist.\n\n");
	}
	return;
}

void cmd_attr(struct CONSOLE *cons, int *fat, char *cmdline)
{
	//struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	struct FILEINFO *finfo = current_dir;
	int i;

	for (i = 6; cmdline[i] != 0; i++){
		if('a' <= cmdline[i] && cmdline[i] <= 'z'){
			cmdline[i] -= 0x20;
		}
	}
	//判断文件是否存在
	int max_index = 16;
	if(current_clustno <= 0){
		max_index = 224;
	}
	struct FILEINFO *is_exsit = file_search(cmdline + 8, finfo, max_index);
	//存在
	if(is_exsit != 0){
		char attr = 0x00;
		switch (cmdline[6])
		{
		case 'R':{attr = 0x01; break;}
		case 'H':{attr = 0x02; break;}
		case 'S':{attr = 0x04; break;}
		case 'A':{attr = 0x20; break;}
		default:{
			cons_putstr0(cons, "Only 4 attr:A,S,H,R.\n\n");
			return;
		}
		}	
		file_chattr(is_exsit, cmdline[5], attr);
	}
	else{	
		cons_putstr0(cons, "File does not exist.\n\n");
	}
}

void cmd_lsattr(struct CONSOLE *cons)
{
	//struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	struct FILEINFO *finfo = current_dir;
	int i, j;
	char s[30];

	int max_index = 16;
	if(current_clustno <= 0){
		max_index = 224;
	}
	for (i = 0; i < max_index; i++) {
		if (finfo[i].name[0] == 0x00) {
			break;
		}
		if (finfo[i].name[0] != 0xe5)
		{
			//文件
			if ((finfo[i].type & 0x18) == 0) {
				sprintf(s, "filename.ext          \n");
				for (j = 0; j < 8; j++) {
					s[j] = finfo[i].name[j];
				}
				s[ 9] = finfo[i].ext[0];
				s[10] = finfo[i].ext[1];
				s[11] = finfo[i].ext[2];

				if ((finfo[i].type & 0x20) != 0){
					s[15] = 'A';
				}
				if ((finfo[i].type & 0x04) != 0){
					s[17] = 'S';
				}
				if ((finfo[i].type & 0x02) != 0){
					s[19] = 'H';
				}
				if ((finfo[i].type & 0x01) != 0){
					s[21] = 'R';
				}
				cons_putstr0(cons, s);
			}
			//目录
			else if((finfo[i].type & 0x28) == 0){
				sprintf(s, "filename DIR    \n");
				for (j = 0; j < 8; j++) {
					s[j] = finfo[i].name[j];
				}
				cons_putstr0(cons, s);
			}
		}
	}
	cons_newline(cons);
	return;
}

void cmd_tree(struct CONSOLE *cons, int clustno, int p)
{
	int i, j;
	char s[30] = "";
	struct FILEINFO *finfo = (struct FILEINFO *)(ADR_DISKIMG + 0x004200 + (clustno - 2) * 512);

	for (i = 0; finfo[i].name[0] != 0x00; i++){
		if (finfo[i].name[0] != 0xe5)
		{
			//文件
			if ((finfo[i].type & 0x18) == 0) {
				//系统文件或隐藏文件不显示
				if ((finfo[i].type & 0x06) == 0) {
					for (j = 0; j < 2*p;j++){
						s[j] = ' ';
					}
					for (j = 0; j < 8;j++){
						s[j+2*p] = finfo[i].name[j];
					}
					s[2*p + 8] = '.';
					s[2*p + 9] = finfo[i].ext[0];
					s[2*p + 10] = finfo[i].ext[1];
					s[2*p + 11] = finfo[i].ext[2];
					cons_putstr0(cons, s);
					cons_newline(cons);
				}
			}
			//目录
			else if((finfo[i].type & 0x28) == 0){
				for (j = 0; j < 2*p;j++){
					s[j] = ' ';
				}
				for (j = 0; j < 8;j++){
					s[j+2*p] = finfo[i].name[j];
				}
				s[2*p + 8] = ' ';
				s[2*p +  9] = 'D';
				s[2*p + 10] = 'I';
				s[2*p + 11] = 'R';
				cons_putstr0(cons, s);
				cons_newline(cons);
				cmd_tree(cons, finfo[i].clustno, p + 1);
			}
		}
	}
	return;
}

char* cmd_path(struct CONSOLE *cons, struct FILEINFO finfo, char *s)
{
	int i, j;
	int clustno = (finfo.reserve[0] << 24) | (finfo.reserve[1] << 16) | (finfo.reserve[2] << 8) | finfo.reserve[3];
	int pos = (finfo.reserve[4] << 24) | (finfo.reserve[5] << 16) | (finfo.reserve[6] << 8) | finfo.reserve[7];

	if (clustno < 0 && pos == 0){
		return "";
	}

	finfo = ((struct FILEINFO *)(ADR_DISKIMG + 0x004200 + (clustno - 2) * 512))[pos];
	cmd_path(cons, finfo, s);
	for (i = 0; s[i] != 0; i++);
	s[i] = '/'; 
	for (j = 0; j < 8 && finfo.name[j] != ' '; j++)
	{
		s[i + 1 + j] = finfo.name[j];
	}
	return s;
}



void cmd_exit(struct CONSOLE *cons, int *fat)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = task_now();
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct FIFO32 *fifo = (struct FIFO32 *) *((int *) 0x0fec);
	if (cons->sht != 0) {
		timer_cancel(cons->timer);
	}
	memman_free_4k(memman, (int) fat, 4 * 2880);
	io_cli();
	if (cons->sht != 0) {
		fifo32_put(fifo, cons->sht - shtctl->sheets0 + 768); /* 768〜1023 */
	} else {
		fifo32_put(fifo, task - taskctl->tasks0 + 1024); /*1024～2023*/
	}
	io_sti();
	for (;;) {
		task_sleep(task);
	}
}

void cmd_start(struct CONSOLE *cons, char *cmdline, int memtotal)
{
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct SHEET *sht = open_console(shtctl, memtotal);
	struct FIFO32 *fifo = &sht->task->fifo;
	int i;
	sheet_slide(sht, 32, 4);
	sheet_updown(sht, shtctl->top);
	/*将命令行输入的字符串逐字复制到新的命令行窗口中*/
	for (i = 6; cmdline[i] != 0; i++) {
		fifo32_put(fifo, cmdline[i] + 256);
	}
	fifo32_put(fifo, 10 + 256);	 /*回车键*/
	cons_newline(cons);
	return;
}

void cmd_ncst(struct CONSOLE *cons, char *cmdline, int memtotal)
{
	struct TASK *task = open_constask(0, memtotal);
	struct FIFO32 *fifo = &task->fifo;
	int i;

	/*将命令行输入的字符串逐字复制到新的命令行窗口中*/
	for (i = 5; cmdline[i] != 0; i++) {
		fifo32_put(fifo, cmdline[i] + 256);
	}
	fifo32_put(fifo, 10 + 256); /*回车键*/
	cons_newline(cons);
	return;
}

void cmd_langmode(struct CONSOLE *cons, char *cmdline)
{
	struct TASK *task = task_now();
	unsigned char mode = cmdline[9] - '0';
	if (mode <= 2) {
		task->langmode = mode;
	} else {
		cons_putstr0(cons, "mode number error.\n");
	}
	cons_newline(cons);
	return;
}

int cmd_app(struct CONSOLE *cons, int *fat, char *cmdline)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct FILEINFO *finfo;
	char name[18], *p, *q;
	struct TASK *task = task_now();
	int i, segsiz, datsiz, esp, dathrb, appsiz;
	struct SHTCTL *shtctl;
	struct SHEET *sht;

	/*根据命令行生成文件名*/
	for (i = 0; i < 13; i++) {
		if (cmdline[i] <= ' ') {
			break;
		}
		name[i] = cmdline[i];
	}
	name[i] = 0; /*暂且将文件名的后面置为0*/

	/*寻找文件 */
	finfo = file_search(name, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	if (finfo == 0 && name[i - 1] != '.') {
		/*由于找不到文件，故在文件名后面加上“.hrb”后重新寻找*/
		name[i    ] = '.';
		name[i + 1] = 'H';
		name[i + 2] = 'R';
		name[i + 3] = 'B';
		name[i + 4] = 0;
		finfo = file_search(name, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	}

	if (finfo != 0) {
		/*如果找到文件*/
		appsiz = finfo->size;
		p = file_loadfile2(finfo->clustno, &appsiz, fat);
		if (appsiz >= 36 && strncmp(p + 4, "Hari", 4) == 0 && *p == 0x00) {
			segsiz = *((int *) (p + 0x0000));
			esp    = *((int *) (p + 0x000c));
			datsiz = *((int *) (p + 0x0010));
			dathrb = *((int *) (p + 0x0014));
			q = (char *) memman_alloc_4k(memman, segsiz);
			task->ds_base = (int) q;
			set_segmdesc(task->ldt + 0, finfo->size - 1, (int) p, AR_CODE32_ER + 0x60);
			set_segmdesc(task->ldt + 1, segsiz - 1, (int) q, AR_DATA32_RW + 0x60); 
			for (i = 0; i < datsiz; i++) {
				q[esp + i] = p[dathrb + i];
			}
			start_app(0x1b, 0 * 8 + 4, esp, 1 * 8 + 4, &(task->tss.esp0));
			shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
			for (i = 0; i < MAX_SHEETS; i++) {
				sht = &(shtctl->sheets0[i]);
				if ((sht->flags & 0x11) == 0x11 && sht->task == task) {
					/*找到被应用程序遗留的窗口*/
					sheet_free(sht); /*关闭*/
				}
			}
			for (i = 0; i < 8; i++) { /*将未关闭的文件关闭*/ 
				if (task->fhandle[i].buf != 0) {
					memman_free_4k(memman, (int) task->fhandle[i].buf, task->fhandle[i].size);
					task->fhandle[i].buf = 0;
				}
			}
			timer_cancelall(&task->fifo);
			memman_free_4k(memman, (int) q, segsiz);
			task->langbyte1 = 0;
		} else {
			cons_putstr0(cons, ".hrb file format error.\n");
		}
		memman_free_4k(memman, (int) p, appsiz);
		cons_newline(cons);
		return 1;
	}
	/*没有找到文件的情况*/
	return 0;
}

int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax)
{
	struct TASK *task = task_now();
	int ds_base = task->ds_base;
	struct CONSOLE *cons = task->cons;
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct SHEET *sht;
	struct FIFO32 *sys_fifo = (struct FIFO32 *) *((int *) 0x0fec);
	int *reg = &eax + 1; /* eax后面的地址*/
	/*强行改写通过PUSHAD保存的值*/
		/* reg[0] : EDI,   reg[1] : ESI,   reg[2] : EBP,   reg[3] : ESP */
		/* reg[4] : EBX,   reg[5] : EDX,   reg[6] : ECX,   reg[7] : EAX */
	int i;
	struct FILEINFO *finfo;
	struct FILEHANDLE *fh;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;

	if (edx == 1) {
		cons_putchar(cons, eax & 0xff, 1);
	} else if (edx == 2) {
		cons_putstr0(cons, (char *) ebx + ds_base);
	} else if (edx == 3) {
		cons_putstr1(cons, (char *) ebx + ds_base, ecx);
	} else if (edx == 4) {
		return &(task->tss.esp0);
	} else if (edx == 5) {
		sht = sheet_alloc(shtctl);
		sht->task = task;
		sht->flags |= 0x10;
		sheet_setbuf(sht, (char *) ebx + ds_base, esi, edi, eax);
		make_window8((char *) ebx + ds_base, esi, edi, (char *) ecx + ds_base, 0);
		sheet_slide(sht, ((shtctl->xsize - esi) / 2) & ~3, (shtctl->ysize - edi) / 2);
		sheet_updown(sht, shtctl->top); /*将窗口图层高度指定为当前鼠标所在图层的高度，鼠标移到上层*/
		reg[7] = (int) sht;
	} else if (edx == 6) {
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		putfonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *) ebp + ds_base);
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
		}
	} else if (edx == 7) {
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		boxfill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	} else if (edx == 8) {
		memman_init((struct MEMMAN *) (ebx + ds_base));
		ecx &= 0xfffffff0; /*以16字节为单位*/
		memman_free((struct MEMMAN *) (ebx + ds_base), eax, ecx);
	} else if (edx == 9) {
		ecx = (ecx + 0x0f) & 0xfffffff0; /*以16字节为单位进位取整*/
		reg[7] = memman_alloc((struct MEMMAN *) (ebx + ds_base), ecx);
	} else if (edx == 10) {
		ecx = (ecx + 0x0f) & 0xfffffff0; /*以16字节为单位进位取整*/
		memman_free((struct MEMMAN *) (ebx + ds_base), eax, ecx);
	} else if (edx == 11) {
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		sht->buf[sht->bxsize * edi + esi] = eax;
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, esi, edi, esi + 1, edi + 1);
		}
	} else if (edx == 12) {
		sht = (struct SHEET *) ebx;
		sheet_refresh(sht, eax, ecx, esi, edi);
	} else if (edx == 13) {
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		hrb_api_linewin(sht, eax, ecx, esi, edi, ebp);
		if ((ebx & 1) == 0) {
			if (eax > esi) {
				i = eax;
				eax = esi;
				esi = i;
			}
			if (ecx > edi) {
				i = ecx;
				ecx = edi;
				edi = i;
			}
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	} else if (edx == 14) {
		sheet_free((struct SHEET *) ebx);
	} else if (edx == 15) {
		for (;;) {
			io_cli();
			if (fifo32_status(&task->fifo) == 0) {
				if (eax != 0) {
					task_sleep(task); /* FIFO为空，休眠并等待*/
				} else {
					io_sti();
					reg[7] = -1;
					return 0;
				}
			}
			i = fifo32_get(&task->fifo);
			io_sti();
			if (i <= 1) { /*光标用定时器*/
				/*应用程序运行时不需要显示光标，因此总是将下次显示用的值置为1*/
				timer_init(cons->timer, &task->fifo, 1); /*下次置为1*/
				timer_settime(cons->timer, 50);
			}
			if (i == 2) { /*光标ON */
				cons->cur_c = COL8_FFFFFF;
			}
			if (i == 3) { /*光标OFF */
				cons->cur_c = -1;
			}
			if (i == 4) { /*只关闭命令行窗口*/
				timer_cancel(cons->timer);
				io_cli();
				fifo32_put(sys_fifo, cons->sht - shtctl->sheets0 + 2024); /*2024～2279*/
				cons->sht = 0;
				io_sti();
			}
			if (i >= 256) { /*键盘数据（通过任务A）等*/
				reg[7] = i - 256;
				return 0;
			}
		}
	} else if (edx == 16) {
		reg[7] = (int) timer_alloc();
		((struct TIMER *) reg[7])->flags2 = 1; /*允许自动取消*/
	} else if (edx == 17) {
		timer_init((struct TIMER *) ebx, &task->fifo, eax + 256);
	} else if (edx == 18) {
		timer_settime((struct TIMER *) ebx, eax);
	} else if (edx == 19) {
		timer_free((struct TIMER *) ebx);
	} else if (edx == 20) {
		if (eax == 0) {
			i = io_in8(0x61);
			io_out8(0x61, i & 0x0d);
		} else {
			i = 1193180000 / eax;
			io_out8(0x43, 0xb6);
			io_out8(0x42, i & 0xff);
			io_out8(0x42, i >> 8);
			i = io_in8(0x61);
			io_out8(0x61, (i | 0x03) & 0x0f);
		}
	} else if (edx == 21) {
		for (i = 0; i < 8; i++) {
			if (task->fhandle[i].buf == 0) {
				break;
			}
		}
		fh = &task->fhandle[i];
		reg[7] = 0;
		if (i < 8) {
			finfo = file_search((char *) ebx + ds_base,
					(struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
			if (finfo != 0) {
				reg[7] = (int) fh;
				fh->size = finfo->size;
				fh->pos = 0;
				fh->buf = file_loadfile2(finfo->clustno, &fh->size, task->fat);
			}
		}
	} else if (edx == 22) {
		fh = (struct FILEHANDLE *) eax;
		memman_free_4k(memman, (int) fh->buf, fh->size);
		fh->buf = 0;
	} else if (edx == 23) {
		fh = (struct FILEHANDLE *) eax;
		if (ecx == 0) {
			fh->pos = ebx;
		} else if (ecx == 1) {
			fh->pos += ebx;
		} else if (ecx == 2) {
			fh->pos = fh->size + ebx;
		}
		if (fh->pos < 0) {
			fh->pos = 0;
		}
		if (fh->pos > fh->size) {
			fh->pos = fh->size;
		}
	} else if (edx == 24) {
		fh = (struct FILEHANDLE *) eax;
		if (ecx == 0) {
			reg[7] = fh->size;
		} else if (ecx == 1) {
			reg[7] = fh->pos;
		} else if (ecx == 2) {
			reg[7] = fh->pos - fh->size;
		}
	} else if (edx == 25) {
		fh = (struct FILEHANDLE *) eax;
		for (i = 0; i < ecx; i++) {
			if (fh->pos == fh->size) {
				break;
			}
			*((char *) ebx + ds_base + i) = fh->buf[fh->pos];
			fh->pos++;
		}
		reg[7] = i;
	} else if (edx == 26) {
		i = 0;
		for (;;) {
			*((char *) ebx + ds_base + i) =  task->cmdline[i];
			if (task->cmdline[i] == 0) {
				break;
			}
			if (i >= ecx) {
				break;
			}
			i++;
		}
		reg[7] = i;
	} else if (edx == 27) {
		reg[7] = task->langmode;
	}
	return 0;
}

int *inthandler0c(int *esp)
{
	struct TASK *task = task_now();
	struct CONSOLE *cons = task->cons;
	char s[30];
	cons_putstr0(cons, "\nINT 0C :\n Stack Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0); /*强制结束程序*/
}

int *inthandler0d(int *esp)
{
	struct TASK *task = task_now();
	struct CONSOLE *cons = task->cons;
	char s[30];
	cons_putstr0(cons, "\nINT 0D :\n General Protected Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0);	/*强制结束程序*/
}

void hrb_api_linewin(struct SHEET *sht, int x0, int y0, int x1, int y1, int col)
{
	int i, x, y, len, dx, dy;

	dx = x1 - x0;
	dy = y1 - y0;
	x = x0 << 10;
	y = y0 << 10;
	if (dx < 0) {
		dx = - dx;
	}
	if (dy < 0) {
		dy = - dy;
	}
	if (dx >= dy) {
		len = dx + 1;
		if (x0 > x1) {
			dx = -1024;
		} else {
			dx =  1024;
		}
		if (y0 <= y1) {
			dy = ((y1 - y0 + 1) << 10) / len;
		} else {
			dy = ((y1 - y0 - 1) << 10) / len;
		}
	} else {
		len = dy + 1;
		if (y0 > y1) {
			dy = -1024;
		} else {
			dy =  1024;
		}
		if (x0 <= x1) {
			dx = ((x1 - x0 + 1) << 10) / len;
		} else {
			dx = ((x1 - x0 - 1) << 10) / len;
		}
	}

	for (i = 0; i < len; i++) {
		sht->buf[(y >> 10) * sht->bxsize + (x >> 10)] = col;
		x += dx;
		y += dy;
	}

	return;
}
