/*
 * lightfs
 * Copyright (c) 2026 oled
 * https://git.sr.ht/~oled/lightfs
 *
 * Licensed under the GNU General Public License v3 (GPL-3.0)
 */

#include <complex.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lightfs.h"


#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#define TRUNCATE _chsize
#else
#include <unistd.h>
#define TRUNCATE ftruncate
#endif

int
file_truncate(FILE * fp, long new_size)
{
	int 		fd = fileno(fp);
	return TRUNCATE(fd, new_size);
}

void
read_dirs(DirBlock * dir, FILE * img)
{
	fread(&dir->meta, sizeof(MetaBlock), 1, img);
	dir->name = malloc(dir->meta.name_size);
	fread(dir->name, 1, dir->meta.name_size, img);
}

void
free_dirs(DirBlock * dir)
{
	free(dir->name);
}

void
seek_dir(FILE * img)
{
	DirBlock 	dir;
	fread(&dir.meta, sizeof(MetaBlock), 1, img);
	fseek(img, dir.meta.name_size, SEEK_CUR);
}

void
read_files(FileBlock * file, FILE * img)
{
	fread(&file->meta, sizeof(MetaBlock), 1, img);
	fread(&file->block, sizeof(FileMetaBlock), 1, img);
	file->name = malloc(file->meta.name_size);
	fread(file->name, 1, file->meta.name_size, img);
	file->data = malloc(file->block.data_size);
	fread(file->data, 1, file->block.data_size, img);
}

void
write_files(FileBlock file, FILE * img)
{
	fwrite(&file.meta, sizeof(MetaBlock), 1, img);
	int 		data_start = (sizeof(MetaBlock) + sizeof(FileMetaBlock) + file.meta.name_size);
	file.block.data_start = data_start;
	fwrite(&file.block, sizeof(FileMetaBlock), 1, img);
	fwrite(file.name, 1, file.meta.name_size, img);
	fwrite(file.data, 1, file.block.data_size, img);
}

void
write_dirs(DirBlock dir, FILE * img)
{
	fwrite(&dir.meta, sizeof(MetaBlock), 1, img);
	fwrite(dir.name, 1, dir.meta.name_size, img);
}

void
free_files(FileBlock * file)
{
	free(file->name);
	free(file->data);
}

void
seek_files(FILE * img)
{
	FileBlock 	file;
	fread(&file.meta, sizeof(MetaBlock), 1, img);
	fread(&file.block, sizeof(FileMetaBlock), 1, img);
	fseek(img, file.meta.name_size, SEEK_CUR);
	fseek(img, file.block.data_size, SEEK_CUR);
}

int
lfs_doffset(LightFS * fs, const char *name, int parent_offset)
{
	FILE           *img = fs->img;

	rewind(img);
	int 		type;

	DirBlock 	dir;
	while (fread(&type, sizeof(int), 1, img) == 1) {
		if (type == TYPEDIR) {
			read_dirs(&dir, img);
			if (dir.meta.isfree == 0 && strcmp(dir.name, name) == 0 && dir.meta.parent_offset == parent_offset) {
				int 		offset = dir.meta.offset;
				free_dirs(&dir);
				return offset;
			}
		} else if (type == TYPEFILE) {
			seek_files(img);
		} else {
			break;
		}
	}
	return -1;
}

int
lfs_foffset(LightFS * fs, const char *filename, int parent_offset)
{
	FILE           *img = fs->img;

	rewind(img);
	int 		type;

	FileBlock 	file;
	while (fread(&type, sizeof(int), 1, img) == 1) {
		if (type == TYPEFILE) {
			read_files(&file, img);
			if (file.meta.isfree == 0 && strcmp(file.name, filename) == 0 && file.meta.parent_offset == parent_offset) {
				int 		offset = file.meta.offset;
				free_files(&file);
				return offset;
			}
			free_files(&file);
		} else if (type == TYPEDIR) {
			seek_dir(img);
		} else {
			break;
		}
	}
	return -1;
}

void
fixoffset(FILE * img, int removed_start, int removed_size)
{
	fseek(img, 0, SEEK_SET);

	while (1) {
		long 		entry_start = ftell(img);
		int 		type;

		if (fread(&type, sizeof(int), 1, img) != 1)
			break;

		if (type == TYPEFILE) {
			FileBlock 	file;
			read_files(&file, img);


			int 		removed_end = removed_start + removed_size;

			if (file.meta.offset >= removed_end)
				file.meta.offset -= removed_size;

			if (file.meta.parent_offset >= removed_end)
				file.meta.parent_offset -= removed_size;


			fseek(img, entry_start + sizeof(int), SEEK_SET);
			write_files(file, img);

			free_files(&file);
		} else if (type == TYPEDIR) {
			DirBlock 	dir;
			read_dirs(&dir, img);


			int 		removed_end = removed_start + removed_size;

			if (dir.meta.offset >= removed_end) {
				dir.meta.offset -= removed_size;
			} else if (dir.meta.offset >= removed_start) {
				dir.meta.offset = -1;
			}
			if (dir.meta.parent_offset >= removed_end) {
				dir.meta.parent_offset -= removed_size;
			} else if (dir.meta.parent_offset >= removed_start) {
				dir.meta.parent_offset = -1;
			}
			fseek(img, entry_start + sizeof(int), SEEK_SET);
			write_dirs(dir, img);

			free_dirs(&dir);
		} else {
			/* fs corruption */
			break;
		}
	}
}


void
shiftIT(LightFS * fs, Shifting shift)
{
	FILE           *img = fs->img;
	rewind(img);
	if (img == NULL) {
		perror("shiftit file open failed");
		return;
	}
	DirBlock 	dir;
	FileBlock 	file;

	fseek(img, 0, SEEK_END);
	int 		filesize = (int) ftell(img);

	if (shift.type == TYPEFILE) {
		int 		offsetoffile = lfs_foffset(fs, shift.name, fs->movement_parent);
		fseek(img, offsetoffile, SEEK_SET);
		int 		shift_file_start = (int) ftell(img) - sizeof(int);
		//int         ->type
				read_files    (&file, img);
		int 		all_size_of_data = sizeof(int) + sizeof(file.meta) + sizeof(file.block) + file.meta.name_size + file.block.data_size;

		int 		end_offset = shift_file_start + all_size_of_data;

		fseek(img, end_offset, SEEK_SET);
		int 		buffer_size = filesize - end_offset;
		if (buffer_size < 0) {
			return;
		}
		char           *buffer = malloc(buffer_size);
		if (!buffer) {
			return;
		}
		fread(buffer, 1, buffer_size, img);

		fseek(img, shift_file_start, SEEK_SET);
		fwrite(buffer, 1, buffer_size, img);

		fflush(img);

		fixoffset(img, shift_file_start, all_size_of_data);
		file_truncate(img, filesize - all_size_of_data);
		free(buffer);
		free_files(&file);
	} else {
		int 		offsetoffolder = lfs_doffset(fs, shift.name, fs->movement_parent);
		fseek(img, offsetoffolder, SEEK_SET);
		int 		shift_folder_start = (int) ftell(img) - sizeof(int);
		//int         ->type
				read_dirs     (&dir, img);
		int 		all_size_of_data = sizeof(int) + sizeof(dir.meta) + dir.meta.name_size;

		int 		end_offset = shift_folder_start + all_size_of_data;

		fseek(img, end_offset, SEEK_SET);
		int 		buffer_size = filesize - end_offset;
		if (buffer_size < 0) {
			return;
		}
		char           *buffer = malloc(buffer_size);
		if (!buffer) {
			return;
		}
		fread(buffer, 1, buffer_size, img);

		fseek(img, shift_folder_start, SEEK_SET);
		fwrite(buffer, 1, buffer_size, img);

		fflush(img);
		fixoffset(img, shift_folder_start, all_size_of_data);
		file_truncate(img, filesize - all_size_of_data);
		free(buffer);
		free_dirs(&dir);
	}

}

void
lfs_rm(LightFS * fs, char *name)
{
	Shifting 	shift;
	shift.name = name;
	shift.type = TYPEFILE;
	shiftIT(fs, shift);
}


void
lfs_rmdir(LightFS * fs, char *name)
{
	int 		offset = lfs_doffset(fs, name, fs->movement_parent);
	int 		curr_offset = fs->movement_parent;
	fs->movement_parent = offset;
	FILE           *img = fs->img;

	int 		type;
	DirBlock 	dir;
	FileBlock 	file;

	char           *files[256];
	char           *dirs[256];
	int 		file_count = 0;
	int 		dir_count = 0;

	rewind(img);

	while (fread(&type, sizeof(int), 1, img) == 1) {
		if (type == TYPEFILE) {
			read_files(&file, img);

			if (!file.meta.isfree &&
			    file.meta.parent_offset == offset &&
			    file_count < 256) {

				files[file_count++] = strdup(file.name);
			}
			free_files(&file);

		} else if (type == TYPEDIR) {
			read_dirs(&dir, img);

			if (!dir.meta.isfree &&
			    dir.meta.parent_offset == offset &&
			    dir_count < 256) {

				dirs[dir_count++] = strdup(dir.name);
			}
			free_dirs(&dir);
		}
	}

	for (int i = 0; i < file_count; i++) {
		Shifting 	s;
		s.type = TYPEFILE;
		s.name = files[i];

		shiftIT(fs, s);
		free(files[i]);
	}

	for (int i = 0; i < dir_count; i++) {
		lfs_rmdir(fs, dirs[i]);
		free(dirs[i]);
	}

	fs->movement_parent = curr_offset;
	Shifting 	s;
	s.type = TYPEDIR;
	s.name = name;
	shiftIT(fs, s);
}



void
lfs_newdir(LightFS * fs, const char *dirname, int parent_offset)
{
	if (lfs_doffset(fs, dirname, parent_offset) != -1) {

		return;
	};
	FILE           *img = fs->img;
	DirBlock 	dir;
	dir.meta.name_size = strlen(dirname) + 1;
	dir.name = malloc(dir.meta.name_size);
	memcpy(dir.name, dirname, dir.meta.name_size);
	dir.meta.isfree = 0;
	dir.meta.parent_offset = parent_offset;
	int 		type = TYPEDIR;

	long 		write_pos;

	write_pos = ftell(img);


	fseek(img, write_pos, SEEK_SET);
	fwrite(&type, sizeof(int), 1, img);
	dir.meta.offset = (int) ftell(img);
	write_dirs(dir, img);
	free(dir.name);
}

void
lfs_newfile(LightFS * fs, const char *filename, char *data, size_t size, int parent_offset)
{
	if (lfs_foffset(fs, filename, parent_offset) != -1) {
		return;
	}
	FILE           *img = fs->img;
	FileBlock 	file;
	file.meta.name_size = strlen(filename) + 1;
	file.name = malloc(file.meta.name_size);
	memcpy(file.name, filename, file.meta.name_size);
	file.block.data_size = size;
	file.data = malloc(file.block.data_size);
	memcpy(file.data, data, file.block.data_size);
	file.meta.parent_offset = parent_offset;
	file.meta.isfree = 0;
	int 		type = TYPEFILE;
	long 		write_pos;


	write_pos = ftell(img);


	fseek(img, write_pos, SEEK_SET);
	fwrite(&type, sizeof(int), 1, img);
	file.meta.offset = (int) ftell(img);
	write_files(file, img);
	free(file.name);
	free(file.data);
}


void
lfs_list(LightFS * fs, ListFF * list)
{
	FILE           *img = fs->img;
	rewind(img);

	DirBlock 	dir;
	FileBlock 	file;
	int 		type;

	list->filescount = 0;
	list->folderscount = 0;
	list->entrycount = 0;

	list->dir = NULL;
	list->file = NULL;
	list->entry = NULL;

	while (fread(&type, sizeof(int), 1, img) == 1) {

		if (type == TYPEDIR) {
			read_dirs(&dir, img);

			if (!dir.meta.isfree &&
			    dir.meta.parent_offset == fs->movement_parent) {

				DirBlock       *d = malloc(sizeof(DirBlock));
				d->meta = dir.meta;
				d->name = strdup(dir.name);

				list->dir = realloc(
						    list->dir,
				sizeof(DirBlock *) * (list->folderscount + 1)
					);
				list->dir[list->folderscount++] = d;

				list->entry = realloc(
						      list->entry,
				sizeof(ListEntry *) * (list->entrycount + 1)
					);
				list->entry[list->entrycount] = malloc(sizeof(ListEntry));
				list->entry[list->entrycount]->type = TYPEDIR;
				list->entry[list->entrycount]->name = strdup(dir.name);
				list->entrycount++;
			}
			free_dirs(&dir);

		} else if (type == TYPEFILE) {
			read_files(&file, img);

			if (!file.meta.isfree &&
			    file.meta.parent_offset == fs->movement_parent) {

				FileBlock      *f = malloc(sizeof(FileBlock));
				f->meta = file.meta;
				f->block = file.block;
				f->name = strdup(file.name);

				f->data = malloc(file.block.data_size);
				memcpy(f->data, file.data, file.block.data_size);

				list->file = realloc(
						     list->file,
				sizeof(FileBlock *) * (list->filescount + 1)
					);
				list->file[list->filescount++] = f;

				list->entry = realloc(
						      list->entry,
				sizeof(ListEntry *) * (list->entrycount + 1)
					);
				list->entry[list->entrycount] = malloc(sizeof(ListEntry));
				list->entry[list->entrycount]->type = TYPEFILE;
				list->entry[list->entrycount]->name = strdup(file.name);
				list->entrycount++;
			}
			free_files(&file);
		}
	}
}

void
lfs_free_list(ListFF * list)
{
	for (int i = 0; i < list->filescount; i++) {
		free(list->file[i]);
	}

	for (int i = 0; i < list->folderscount; i++) {
		free(list->dir[i]);
	}

	for (int i = 0; i < list->entrycount; i++) {
		free(list->entry[i]->name);
		free(list->entry[i]);
	}

	free(list->file);
	free(list->dir);
	free(list->entry);

	list->file = NULL;
	list->dir = NULL;
	list->entry = NULL;

	list->filescount = 0;
	list->folderscount = 0;
	list->entrycount = 0;
}


void
lfs_cat(LightFS * fs, const char *filename, int parent_offset, char **out, size_t * size)
{
	FILE           *img = fs->img;
	int 		type;
	FileBlock 	file;

	*out = NULL;

	rewind(img);

	while (fread(&type, sizeof(int), 1, img) == 1) {
		if (type == TYPEFILE) {
			read_files(&file, img);

			if (!file.meta.isfree &&
			    file.meta.parent_offset == parent_offset &&
			    strcmp(file.name, filename) == 0) {

				*out = malloc(file.block.data_size + 1);
				memcpy(*out, file.data, file.block.data_size);
				(*out)[file.block.data_size] = '\0';
				*size = file.block.data_size;

				free_files(&file);
				return;
			}
			free_files(&file);

		} else if (type == TYPEDIR) {
			seek_dir(img);
		}
	}
}


void
lfs_cd(LightFS * fs, char *foldername)
{
	FILE           *img = fs->img;

	rewind(img);

	DirBlock 	dir;
	int 		type;
	while (fread(&type, sizeof(int), 1, img) == 1) {
		if (type == TYPEDIR) {
			read_dirs(&dir, img);
			if (strcmp(foldername, "..") == 0) {
				if (dir.meta.offset == fs->movement_parent) {
					fs->movement_parent = dir.meta.parent_offset;
					free_dirs(&dir);
					return;
				}
			} else if (dir.meta.isfree == 0 && dir.meta.parent_offset == fs->movement_parent &&
				   strcmp(dir.name, foldername) == 0) {
				fs->old_parent = fs->movement_parent;
				fs->movement_parent = dir.meta.offset;
				free_dirs(&dir);
				return;
			}
			free_dirs(&dir);
		} else if (type == TYPEFILE) {
			seek_files(img);
		}
	}
	printf("Folder '%s' not found.\n", foldername);
}

void
lfs_go_path(LightFS * fs, char *path)
{
	int 		path_timer = 0;
	int 		outpathoffset = 0;
	char           *folder = strtok(path, "/");
	while (folder != NULL) {
		lfs_cd(fs, folder);
		folder = strtok(NULL, "/");
	}
}

void
lfs_pwd(LightFS * fs, char *pwdout)
{
	FILE           *img = fs->img;
	rewind(img);

	DirBlock 	dir;
	int 		type;
	char 		path     [1024] = "";
	int 		curroffset = fs->movement_parent;

	while (curroffset != 0) {
		fseek(img, 0, SEEK_SET);
		while (fread(&type, sizeof(type), 1, img) == 1) {
			if (type == TYPEDIR) {
				read_dirs(&dir, img);
				if (dir.meta.offset == curroffset) {
					char 		temp     [1024];
					snprintf(temp, sizeof(temp), "/%s%s", dir.name, path);
					strcpy(path, temp);
					curroffset = dir.meta.parent_offset;
					free_dirs(&dir);
					break;
				}
				free_dirs(&dir);
			} else if (type == TYPEFILE) {
				seek_files(img);
			}
		}
	}

	strcpy(pwdout, path + 1);

}
