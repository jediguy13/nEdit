#include "file_browser.h"

vstring * current_directory_header = NULL;
vstring * current_directory = NULL;

vstring * filename_header = NULL;
vstring * filename = NULL;

clock_t last_clock_fb = 0;
clock_t fb_keypress_duration[FB_ACTION_LEN] = {0};

void set_current_directory_string() {
	char cd[FILENAME_MAX];
	getcwd(cd, sizeof(cd));
	freeinn(current_directory->str);
	current_directory->len = 0;
	while(cd[current_directory->len] != '\0') {
		current_directory->len++;
	}
	current_directory->str = malloc(sizeof(char) * current_directory->len);
	strcpy(current_directory->str, cd);
}

void open_directory(screen * sc, const char * fqdir) {
	DIR *dir;
	struct dirent *ent;

	line_t * head = malloc(sizeof(line_t));
	init_line_t_ptr(head);
	line_t * tail = head;

	if ((dir = opendir(fqdir)) != NULL) {
		while ((ent = readdir(dir)) != NULL ) {
			printf(ent->d_name);
			printf("\n");
			tail->vstr = malloc(sizeof(vstring));
			tail->vstr->len = strlen(ent->d_name);
			tail->vstr->str = malloc(sizeof(char)*(tail->vstr->len+1));
			strcpy(tail->vstr->str, ent->d_name);
			line_t * new_ent = malloc(sizeof(line_t));
			init_line_t_ptr(new_ent);
			insert_after(tail, new_ent);
			tail = new_ent;
		}
		tail = tail->prev;
		// An empty node's vstr never has an str initialized
		//free(tail->next->vstr->str);
		free(tail->next->vstr);
		free(tail->next);
		tail->next = NULL;
		closedir(dir);

		recurse_free_lines(sc);
		sc->topmost_line = head;
		sc->current_line = head;
		
		chdir(fqdir);
		set_current_directory_string();
		draw_fb(sc);
	}

}

void set_filename(screen * sc, char * name) {
	if (sc->fqfilename != NULL) {
		if (sc->fqfilename->str != NULL) {
			free(sc->fqfilename->str);
		}
	} else {
		sc->fqfilename = malloc(sizeof(vstring));
	}
	sc->fqfilename->len = strlen(name);
	sc->fqfilename->str = malloc(sizeof(char) * (sc->fqfilename->len + 1));
	strcpy(sc->fqfilename->str, name);
}

void init_fb(screen * sc) {

	sc->mode = 1;

	current_directory_header = malloc(sizeof(vstring));
	current_directory_header->str = "Current Directory:";
	current_directory_header->len = 18;
	
	current_directory = malloc(sizeof(vstring));
	current_directory->str = NULL;
	set_current_directory_string();
	
	filename_header = malloc(sizeof(vstring));
	filename_header->str = "Filename:";
	filename_header->len = 9;

	filename = malloc(sizeof(vstring));
	filename->str = malloc(sizeof(char) * 2);
	filename->str[0] = '.';
	filename->str[1] = '\0';
	filename->len = 1;

	open_directory(sc, current_directory->str);
}

void deinit_fb(screen * sc) {
	
//	freeinn(current_directory_header->str);
	freeinn(current_directory_header);
	
	freeinn(current_directory->str);
	freeinn(current_directory);
	
//	freeinn(filename_header->str);
	freeinn(filename_header);
	
	freeinn(filename->str);
	freeinn(filename);
	
	recurse_free_lines(sc);
}

void move_fb_cursor_up(screen * sc) {
	if (sc->mode == 2) {
		toggle_filename_browsing(sc);
	} else {
		if (sc->current_line->prev != NULL) {
			if(sc->cursor_row > 0) {
				sc->cursor_row--;
			} else {
				sc->topmost_line = sc->topmost_line->prev;
			}
			sc->current_line = sc->current_line->prev;
			draw_fb(sc);
		}
	}
}

void move_fb_cursor_down(screen * sc) {
	if (sc->current_line->next != NULL) {
		if (sc->cursor_row < NUM_ROWS - LINES_ABOVE - LINES_BELOW) {
			sc->cursor_row++;
		} else {
			sc->topmost_line = sc->topmost_line->next;
		}
		sc->current_line = sc->current_line->next;
		draw_fb(sc);
	}
}

void move_fb_cursor_left(screen * sc) {
	if (sc->mode == 1) {
		toggle_filename_browsing(sc);
	}
	
	// unsigned int acc = sc->actual_cursor_col;
	unsigned int old_col = sc->displ_cursor_col;

	if (sc->actual_cursor_col > 0) {
		sc->actual_cursor_col--;
	}
	if (sc->actual_cursor_col < sc->leftmost_index) {
		sc->leftmost_index = sc->actual_cursor_col;
		draw_fb(sc);
	}
	sc->displ_cursor_col = sc->actual_cursor_col - sc->leftmost_index;
	update_cursor(sc, NUM_ROWS-1, old_col, NUM_ROWS-1, sc->displ_cursor_col, 0xffff, 0x0000);
}

void move_fb_cursor_right(screen * sc) {
	if (sc->mode == 1) {
		toggle_filename_browsing(sc);
	}

	unsigned int old_col = sc->displ_cursor_col;

	if (sc->actual_cursor_col < filename->len) {
		sc->actual_cursor_col++;
	}

	if (sc->actual_cursor_col > sc->leftmost_index + NUM_COLS - 1) {
		sc->leftmost_index = sc->actual_cursor_col - NUM_COLS + 1;
		draw_fb(sc);
	}
	sc->displ_cursor_col = sc->actual_cursor_col - sc->leftmost_index;
	update_cursor(sc, NUM_ROWS-1, old_col, NUM_ROWS-1, sc->displ_cursor_col, 0xffff, 0x0000);
}

void fb_delete_before_cursor(screen * sc) {
	if (sc->actual_cursor_col) {
		vstring * st = filename;
		uint16_t ac = sc->actual_cursor_col;
		uint16_t x=0;
		char * new_buffer = malloc(st->len*sizeof(char));
		for ( ; x<ac-1; x++) {
			new_buffer[x] = st->str[x];
		}
		for( ; x<st->len-1; x++ ) {
			new_buffer[x] = st->str[x+1];
		}
		new_buffer[x] = '\0';
		free(st->str);
		st->str = new_buffer;
		st->len--;
		draw_fb(sc);
		move_fb_cursor_left(sc);
	}	
}


void toggle_filename_browsing(screen * sc) {
	if (sc->mode == 1) {
		//Stuff
		sc->mode = 2;
	}
	else if (sc->mode == 2) {
		//Stuff
		sc->mode = 1;
	}
	draw_fb(sc);
}



void fb_enter_pressed(screen * sc) {
	printf("Enter pressed\n");
	if (sc->mode == 1) {
		printf("Is in selecting mode\n");
		vstring fqfn = concat(*current_directory, *sc->current_line->vstr);
		printf("%s\n", fqfn.str);
		printf("Did a concat\n");
		struct stat statbuf;
		stat(fqfn.str, &statbuf);
		if (!S_ISREG(statbuf.st_mode)) {
			printf("Trying to open directory %s\n", fqfn.str);
			open_directory(sc, fqfn.str);
		} else {
			printf("Switching to editing the file name\n");
			if (filename != NULL) {
				if (filename->str != NULL) {
					free(filename->str);
				}
			} else {
				filename = malloc(sizeof(vstring));
			}
			filename->len = sc->current_line->vstr->len;
			filename->str = malloc(sizeof(char) * (filename->len + 1));
			strcpy(filename->str, sc->current_line->vstr->str);
			toggle_filename_browsing(sc);
		}
	}
	else if (sc->mode == 2) {
		printf("Is in filename editing mode\n");
		vstring fqfn = concat(*current_directory, *filename);
		printf("%s\n", fqfn.str);
		printf("Did a concat\n");
		struct stat statbuf;
		int exists = stat(fqfn.str, &statbuf);
		if(!S_ISREG(statbuf.st_mode) && exists == 0) {
			open_directory(sc, fqfn.str);
		} else {
			printf("Opening file sequence\n");
			free(sc->fqfilename);

			sc->fqfilename = malloc(sizeof(vstring));
			sc->fqfilename->str = malloc(sizeof(char) * (fqfn.len + 1));
			
			strcpy(sc->fqfilename->str, fqfn.str);
			sc->fqfilename->len = fqfn.len;
			
			vstring data = read_file(fqfn.str);
			
			deinit_fb(sc);
			if (data.str == NULL) {
				data.str = "\n";
				data.len = 1;
			}
			printf("Now loading text\n");
			load_text(sc, data);
			printf("Loaded text?\n");
			sc->mode = 0;
			sc->leftmost_index = 0;
			sc->cursor_row = 0;
			sc->actual_cursor_col = 0;
			sc->ideal_cursor_col = 0;
			sc->displ_cursor_col = 0;
		}
	}
}

void draw_fb(screen * sc) {
	memset(sc->buffer, 0, 320*240*sizeof(uint16_t));
	unsigned int x;
	unsigned int y;
	y=0;
	for(x=0; x<NUM_COLS && x<current_directory_header->len; x++) {
		draw_char(sc, y, x, current_directory_header->str[x], 0xffff, 0);
	}
	y=1;
	for(x=0; x<NUM_COLS && x<current_directory->len; x++) {
		draw_char(sc, y, x, current_directory->str[x], 0xffff, 0);
	}
	line_t * cl = sc->topmost_line;
	for(y=2; y<=NUM_ROWS-LINES_ABOVE-LINES_BELOW && cl != NULL; y++) {
		for(x=0; x<NUM_COLS && x<cl->vstr->len; x++) {
			if (sc->current_line == cl) {
				draw_char(sc, y, x, cl->vstr->str[x], 0, 0xffff);
			} else {
				draw_char(sc, y, x, cl->vstr->str[x], 0xffff, 0);
			}
		}
		cl = cl->next;
	}
	y = NUM_ROWS - 2;
	for(x=0; x<NUM_COLS && x<filename_header->len; x++) {
		draw_char(sc, y, x, filename_header->str[x], 0xffff, 0);
	}
	y++;
	for(x=0; x<NUM_COLS && x+sc->leftmost_index<filename->len; x++) {
		draw_char(sc, y, x, filename->str[sc->leftmost_index+x], 0xffff, 0);
	}
	lcd_blit(sc->buffer, sc->scr_type);
}

void fb_insert_char(screen * sc, uint16_t x, char mode) {
	if (sc->mode == 1) {
		toggle_filename_browsing(sc);
	}
	switch(mode) {
		case 0:
			insert_after_cursor(filename, normal_keypress[x], sc->actual_cursor_col);
			break;
		case 1:
			insert_after_cursor(filename, shift_keypress[x], sc->actual_cursor_col);
			break;
		case 2:
			insert_after_cursor(filename, ctrl_keypress[x], sc->actual_cursor_col);
			break;
	}
	draw_fb(sc);
	move_fb_cursor_right(sc);
}

void fb_scan_keys(screen * sc) {
	clock_t cur_clock = clock();
	
	clock_t diff;
	
	if (cur_clock > last_clock_fb) {
		diff = cur_clock - last_clock_fb;
	} else {
		diff = 1;
	}
	
	for(uint16_t x=0; x<FB_ACTION_LEN; x++) {
		if (isKeyPressed(FB_BUTTONS[x])) {
			if (!fb_keypress_duration[x]) {
				FB_FUNCS[x](sc);
			}
			if (fb_keypress_duration[x] > ED_INITIAL_LOOP_COUNT) {
				FB_FUNCS[x](sc);
				fb_keypress_duration[x] = ED_RESET_LOOP_COUNT;
			}
			fb_keypress_duration[x] += diff;
		} else {
			fb_keypress_duration[x] = 0;
		}
	}

	char mode = isKeyPressed(KEY_NSPIRE_SHIFT) ? 1 : isKeyPressed(KEY_NSPIRE_CTRL) ? 2 : 0;

	for(uint16_t x=0; x<INSERT_ACTION_LEN; x++) {
		if (isKeyPressed(INSERT_BUTTONS[x])) {
			if (!insert_keypress_duration[x]) {
				fb_insert_char(sc, x, mode);
			}
			if (insert_keypress_duration[x] > IN_INITIAL_LOOP_COUNT) {
				fb_insert_char(sc, x, mode);
				insert_keypress_duration[x] = IN_RESET_LOOP_COUNT;
			}
			insert_keypress_duration[x] += diff;
		} else {
			insert_keypress_duration[x] = 0;
		}
	}
	
	last_clock_fb = cur_clock;
}
