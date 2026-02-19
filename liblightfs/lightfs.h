/*
 * lightfs
 * Copyright (c) 2026 0l3d
 * https://github.com/0l3d/lightfs
 *
 * Licensed under the GNU General Public License v3 (GPL-3.0)
 */

#ifndef DEFINE_LIGHTFS
#define DEFINE_LIGHTFS

#include <stdio.h>

#define TYPEDIR 1
#define TYPEFILE 2

typedef struct {
	FILE           *img;
	int 		movement_parent;
	int 		old_parent;
} 		LightFS;

typedef struct {
	int 		name_size;
	int 		offset;
	int 		isfree;
	int 		parent_offset;
} 		MetaBlock;

typedef struct {
	char           *name;
	int 		type;
} 		Shifting;

typedef struct {
	MetaBlock 	meta;
	char           *name;
} 		DirBlock;

typedef struct {
	int 		data_size;
	int 		data_start;
} 		FileMetaBlock;

typedef struct {
	MetaBlock 	meta;
	char           *name;
	FileMetaBlock 	block;
	char           *data;
} 		FileBlock;

typedef struct {
	int 		type;
	char           *name;
} 		ListEntry;

typedef struct {
	int 		filescount;
	int 		folderscount;
	int 		entrycount;
	DirBlock      **dir;
	FileBlock     **file;
	ListEntry     **entry;
} 		ListFF;

int 		lfs_doffset(LightFS * fs, const char *name, int parent_offset);
int 		lfs_foffset(LightFS * fs, const char *name, int parent_offset);

void 		lfs_newdir(LightFS * fs, const char *name, int parent_offset);
void 		lfs_newfile(LightFS * fs, const char *name, char *data, int parent_offset);

void 		lfs_list (LightFS * fs, ListFF * list);
void 		lfs_free_list(ListFF * list);
void 		lfs_cd   (LightFS * fs, char *name);
void 		lfs_pwd  (LightFS * fs, char *out);
void 		lfs_go_path(LightFS * fs, char *path);

void 		lfs_cat  (LightFS * fs, const char *filename, int parent_offset, char **out, size_t *size);

void 		lfs_rm   (LightFS * fs, char *name);
void 		lfs_rmdir(LightFS * fs, char *name);

#endif
