#include "./liblightfs/lightfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define IMG "fs.img"

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
			perror("Failed to open LightFS image");
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
			FILE           *fp = fopen(file_name, "rb");
			if (fp == NULL) {
				perror("addfile failed");
				free(file_name);
				return 1;
			}
			fseek(fp, 0, SEEK_END);
			long 		size = ftell(fp);
			rewind(fp);

			if (size <= 0) {
				fclose(fp);
				free(file_name);
				return 1;
			}
			unsigned char  *buf = malloc(size);
			if (!buf) {
				perror("malloc");
				fclose(fp);
				free(file_name);
				return 1;
			}
			fread(buf, 1, size, fp);
			fclose(fp);
			char           *folderpath = NULL;
			token = strtok(NULL, " ");
			if (token) {
				folderpath = strdup(token);
				if (!folderpath) {
					perror("strdup");
					free(buf);
					free(file_name);
					return 1;
				}
			}
			char           *end_foldername = NULL;

			if (folderpath && strchr(folderpath, '/')) {
				char           *tmp = strdup(folderpath);
				if (!tmp) {
					perror("strdup");
					free(folderpath);
					free(buf);
					free(file_name);
					return 1;
				}
				char           *t = strtok(tmp, "/");
				char           *last = NULL;
				while (t) {
					last = t;
					t = strtok(NULL, "/");
				}
				if (last)
					end_foldername = strdup(last);
				lfs_go_path(&fs, folderpath);
				lfs_cd(&fs, "..");

				free(tmp);
			}
			lfs_newfile(
				    &fs,
				    file_name,
				    buf,
				    lfs_doffset(&fs,
			       end_foldername ? end_foldername : folderpath,
						fs.movement_parent
						)
				);
			free(folderpath);
			free(end_foldername);
			free(file_name);
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
				lfs_cat(&fs, file, fs.movement_parent, &out);
				printf("%s\n", out);
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
				lfs_newfile(&fs, name, buffer, fs.movement_parent);
			} else {
				printf("undefined file or data\n");
			}
		} else if (strcmp(command, "rmdir") == 0) {
			char           *name = strtok(NULL, " ");
			if (name) {
				lfs_rmdir(&fs, name);
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
