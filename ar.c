#include "./liblightfs/lightfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define VERSION_INFO "v0.1"

int
file_writer(LightFS * fs, char *file_name, char *folderpath)
{
	FILE           *fp = fopen(file_name, "rb");
	if (!fp) {
		perror("addfile failed");
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	long 		size = ftell(fp);
	rewind(fp);

	if (size <= 0) {
		fclose(fp);
		return 1;
	}
	unsigned char  *buf = malloc(size);
	if (!buf) {
		fclose(fp);
		return 1;
	}
	fread(buf, 1, size, fp);
	fclose(fp);

	char           *base = strrchr(file_name, '/');
	base = base ? base + 1 : file_name;

	int 		parent_offset;
	if (folderpath && strlen(folderpath) == 0)
		folderpath = NULL;

	if (folderpath) {

		if (strcmp(folderpath, "/") == 0) {
			parent_offset = 0;
			//root
		} else {
			parent_offset =
				lfs_doffset(fs, folderpath, fs->movement_parent);
		}

	} else {
		parent_offset = fs->movement_parent;
	}

	lfs_newfile(fs, base, buf, size, parent_offset);

	free(buf);
	return 0;
}



//DIRENT OUTPUT
// TYPE 8->FILE
// TYPE 4->FOLDER
void
folder_file_handler(LightFS * fs, char *folderpath, int offset)
{
	lfs_newdir(fs, folderpath, offset);

	DIR            *dirp = opendir(folderpath);
	if (!dirp) {
		perror("opendir");
		return;
	}
	struct dirent  *dp;
	while ((dp = readdir(dirp)) != NULL) {

		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;

		char 		path     [1024];
		snprintf(path, sizeof(path), "%s/%s",
			 folderpath, dp->d_name);
		if (dp->d_type == DT_DIR) {
			folder_file_handler(
					    fs,
					    path,
					 lfs_doffset(fs, folderpath, offset)
				);
		} else {
			file_writer(fs, path, folderpath);
		}
	}

	closedir(dirp);
}

void 
file_extracter(LightFS * fs, char *name, int parent_offset)
{
	char           *out;
	size_t 		size;
	lfs_cat(fs, name, parent_offset, &out, &size);

	FILE           *file = fopen(name, "wb");
	if (!file) {
		perror("file extracter fopen error");
		return;
	}
	
	printf("extracting %s, size = %lu\n", name, size);
	
	fwrite(out, 1, size, file);

	fclose(file);

	free(out);

}


void 
fs_file_folder_handler(LightFS * fs, char *name, int parent_offset)
{
	int curr_off = fs->movement_parent;
	int 		dofset = lfs_doffset(fs, name, parent_offset);
	int 		fofset = lfs_foffset(fs, name, parent_offset);
	if (dofset != -1) {
		mkdir(name, 0755);
		lfs_cd(fs, name);
		chdir(name);
	}
	ListFF 		f;
	lfs_list(fs, &f);
	for (int i = 0; i < f.filescount; i++) {
		file_extracter(fs, f.file[i]->name, fs->movement_parent);
		if (fofset != -1) break;
	}
	for (int i = 0; i < f.folderscount; i++) {
		if (fofset != -1)
			break;
		fs_file_folder_handler(fs, f.dir[i]->name, fs->movement_parent);
	}
	lfs_free_list(&f);
	fs->movement_parent = curr_off;
}

int
main(int argc, char **argv)
{
	LightFS 	fs;

	if (argv[1]) {
		char           *dot = strrchr(argv[1], '.');
		if (!dot || strcmp(dot + 1, "arx") != 0) {
			printf("This file isn't an AR file.\n");
			return 1;
		}
	}
	fs.img = fopen(argv[1], "r+b");
	if (!fs.img) {
		fs.img = fopen(argv[1], "w+b");
		if (!fs.img) {
			perror("Failed to open LightFS image.");
			return 1;
		}
	}
	fs.movement_parent = 0;
	fs.old_parent = 0;


	int 		loop = 1;
	while (loop) {
		char 		pwdout   [256];
		lfs_pwd(&fs, pwdout);
		printf("/%s -> ", pwdout);

		char 		input    [512];
		if (fgets(input, sizeof(input), stdin) == NULL) {
			continue;
		}
		input[strcspn(input, "\n")] = 0;

		char           *command = strtok(input, " ");
		if (!command)
			continue;

		if (strcmp(command, "ls") == 0) {
			ListFF 		f;
			lfs_list(&fs, &f);
			for (int i = 0; i < f.entrycount; i++) {
				if (f.entry[i]->type == TYPEDIR) {
					printf("[DIR] %s\n", f.entry[i]->name);
				} else {
					printf("[FILE] %s\n", f.entry[i]->name);
				}
			}
			lfs_free_list(&f);
		} else if (strcmp(command, "addfile") == 0) {

			char           *token = strtok(NULL, " ");
			if (!token) {
				fprintf(stderr, "missing filename\n");
				return 1;
			}
			char           *file_name = strdup(token);
			if (!file_name) {
				perror("strdup");
				return 1;
			}
			char           *folderpath = NULL;
			token = strtok(NULL, " ");
			if (token) {
				folderpath = strdup(token);
				if (!folderpath) {
					perror("strdup");
					free(file_name);
					return 1;
				}
			}
			file_writer(&fs, file_name, folderpath);

			free(folderpath);
			free(file_name);
		} else if (strcmp(command, "addfolder") == 0) {
			char           *folderpath = strtok(NULL, " ");
			folder_file_handler(&fs, folderpath, fs.movement_parent);
		} else if (strcmp(command, "extract") == 0) {
			char           *item_name = strtok(NULL, " ");
			fs_file_folder_handler(&fs, item_name, fs.movement_parent);
		} else if (strcmp(command, "exit") == 0) {
			loop = 0;
		} else if (strcmp(command, "cd") == 0) {
			char           *folder = strtok(NULL, " ");
			if (strchr(folder, '/')) {
				lfs_go_path(&fs, folder);
			} else if (folder) {
				lfs_cd(&fs, folder);
			} else {
				printf("undefined folder\n");
			}
		} else if (strcmp(command, "cat") == 0) {
			char           *file = strtok(NULL, " ");
			if (file) {
				char           *out;
				size_t 		size;
				lfs_cat(&fs, file, fs.movement_parent, &out, &size);
				printf("%s\nSIZE (bytes): %lu\n", out, size);
				free(out);
			} else {
				printf("undefined file\n");
			}
		} else if (strcmp(command, "mkdir") == 0) {
			char           *name = strtok(NULL, " ");
			if (name) {
				lfs_newdir(&fs, name, fs.movement_parent);
			} else {
				printf("undefined name\n");
			}
		} else if (strcmp(command, "new") == 0) {
			char           *name = strtok(NULL, " ");
			char           *datapart = strtok(NULL, " ");
			if (name && datapart) {
				char 		buffer   [4096] = "";
				while (datapart) {
					strcat(buffer, datapart);
					strcat(buffer, " ");
					datapart = strtok(NULL, " ");
				}
				lfs_newfile(&fs, name, buffer, strlen(buffer), fs.movement_parent);
			} else {
				printf("undefined file or data\n");
			}
		} else if (strcmp(command, "rmdir") == 0) {
			char           *name = strtok(NULL, " ");
			if (name) {
				int 		offset = lfs_doffset(&fs, "liblightfss", fs.movement_parent);
				if (offset != -1) {
					lfs_rmdir(&fs, name);
				}
			} else {
				printf("undefined folder\n");
			}
		} else if (strcmp(command, "rm") == 0) {
			char           *name = strtok(NULL, " ");
			if (name) {
				lfs_rm(&fs, name);
			} else {
				printf("undefined file\n");
			}
		} else {
			printf("unrecognized command: %s\n", command);
		}
	}

	fclose(fs.img);
	return 0;
}
