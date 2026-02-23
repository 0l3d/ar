#include "./liblightfs/lightfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define VERSION_INFO "v0.1"

#define HELP_MESSAGE "AR archive manager for .arx. \n" \
					 "Usage: ar [options][options arguments] <file>.arx.<compression algorithm>\n" \
					 "-v	version info \n" \
					 "-h 	help message \n" \
					 "-e	extract \n" \
					 "-c	create archive from folder \n" \
					 "-x	compress with xz or decompress on extract (.xz)\n" \
					 "-g	compress with gzip or decompress on extract (.gz)\n" \
					 "-l	compress with lz4 or decompress on extract (.lz4)\n" \
					 "-n	*Navigate inside of archive with unix-like commands\n\n" \
					 "For compressed files, specify the algorithm.\n" \
					 "For example for extract: ar -ex test.arx.xz OR ar -eg test.arx.gz\nFor example for create: ar -xc files test.arx # out: test.arx.xz \n"  

#define FS_HELP "AR archive manager for .ARX\n" \
				"For CLI: -h\n" \
				"Commands: \n" \
				"ls	- list files/folders.\n" \
				"cat - displays file contents. [filename]\n" \
				"rm - remove file. [filename]\n" \
				"rmdir - remove folder. [foldername]\n" \
				"addfile - add new file. [filepath]\n" \
				"addfolder - add new folder. [folderpath]\n" \
				"extract - extract file/folder. [folder/filename]\n" \
				"cd - change dir. [foldername]\n" \
				"mkdir - create new dir. [foldername]\n" \
				"new - create new file. [filename] [file contents]\n" \
				"exit\n" \
				"help\n" \
				"version\n"

int
file_writer(LightFS * fs, char *file_name, int parent_offset)
{
	FILE           *fp = fopen(file_name, "rb");
	if (!fp) {
		perror("filewriter failed");
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

	lfs_newfile(fs, file_name, buf, size, parent_offset);

	free(buf);
	return 0;
}


void
folder_file_handler(LightFS * fs, char *folderpath, int parent_offset)
{
	DIR            *dirp = opendir(folderpath);
	if (!dirp) {
		perror("opendir");
		return;
	}
	
	char old_dir[PATH_MAX];
	if (!getcwd(old_dir, sizeof(old_dir))) {
		perror("getcwd olddir");
		return;
	}
	int curr_off = fs->movement_parent;
	lfs_newdir(fs, folderpath, fs->movement_parent);
	lfs_cd(fs, folderpath);
	chdir(folderpath);
	struct dirent  *dp;
	while ((dp = readdir(dirp)) != NULL) {

		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;

		if (dp->d_type == DT_DIR) {
			folder_file_handler(fs, dp->d_name, fs->movement_parent);
		} else {
			file_writer(fs, dp->d_name, fs->movement_parent);
		}
	}
	fs->movement_parent = curr_off;
	chdir(old_dir);
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
	 	int 	dofset = lfs_doffset(fs, name, parent_offset);
	 		int fofset = lfs_foffset(fs, name, parent_offset);
	char old_dir[PATH_MAX];
	if (!getcwd(old_dir, sizeof(old_dir))) {
		perror("getcwd olddir");
		return;
	}

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
	chdir(old_dir);
}

void cli_extracter(LightFS *fs, int parent_offset) {
	int curr_off = fs->movement_parent;
	fs->movement_parent = parent_offset;
	ListFF 		f;
	lfs_list(fs, &f);
	for (int i = 0; i < f.filescount; i++) {
		file_extracter(fs, f.file[i]->name, fs->movement_parent);
	}
	for (int i = 0; i < f.folderscount; i++) {
		lfs_cd(fs, f.dir[i]->name);
		mkdir(f.dir[i]->name, 0755);
		chdir(f.dir[i]->name);
		cli_extracter(fs, f.dir[i]->meta.offset);
	}
	lfs_free_list(&f);
	fs->movement_parent = curr_off;
}

int
main(int argc, char **argv)
{
	LightFS 	fs;
	
	if (argc < 1) {
		fprintf(stderr, "AR Needs arguments, -h for help.");
		return 1;
	}
	
	int ch, nav = 0, cre = 0, exr = 0, gzip = 0, xz = 0, lz4 = 0;
	char *cre_folders = NULL;

	while ((ch = getopt(argc, argv, "vhgxlenc:")) != -1) {
		switch (ch) {
		case 'h':
			printf("%s", HELP_MESSAGE);
			return 0;
			break;
		case 'v':
			printf("ar version is %s\n", VERSION_INFO);
			return 0;
			break;
		case 'n':
			nav = 1;
			break;
		case 'g':
			gzip = 1;
			break;
		case 'x':
			xz = 1; 
			break;
		case 'l':
			lz4 = 1;
			break;
		case 'e':
			exr = 1;
			break;
		case 'c':
			cre = 1;
			cre_folders = strdup(optarg);
			break;
		default:
			break;
		}
	}
	
	if (optind >= argc) {
		fprintf(stderr, "-h for help.");
		return 1;
	}
	
	char *file = argv[optind];

	fs.movement_parent = 0;
	fs.old_parent = 0;
	if (!file) {
		printf("-h for help.\n");
		return 1;
	}
	fs.img = fopen(file, "r+b");
	if (!fs.img) {
		fs.img = fopen(file, "w+b");
		if (!fs.img) {
			perror("Failed to open LightFS image.");
			return 1;
		}
	}

	if (nav == 1 && exr != 1 && cre != 1) {
		printf("type help for help\n");
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
		} else if (strcmp(command, "help") == 0) {
			printf("%s\n", FS_HELP);
		} else if (strcmp(command, "version") == 0) {
			printf("ar version is %s\n", VERSION_INFO);
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
			file_writer(&fs, file_name, fs.movement_parent);

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
				int 		offset = lfs_doffset(&fs, name, fs.movement_parent);
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
	}
	
	if (nav != 1 && exr == 1) {
		printf("extracting...\n");
		if (gzip == 1) {
			int size = strlen("gzip ") + strlen(" -d ") + strlen(file);
			char *full_command = malloc(size);
			snprintf(full_command,  size, "gzip -d %s", file);
			system(full_command);
			free(full_command);
		} 
		else if (xz == 1) {
			int size = strlen("xz ") + strlen(" -d ") + strlen(file);
			char *full_command = malloc(size);
			snprintf(full_command,  size, "xz -d %s", file);
			system(full_command);
			free(full_command);

		} 
		else if (lz4 == 1) {
			int size = strlen("lz4 ") + strlen(" -d ") + strlen(file);
			char *full_command = malloc(size);
			snprintf(full_command,  size, "lz4 -d %s", file);
			system(full_command);
			free(full_command);

		}
		cli_extracter(&fs, 0);
	}
	
	if (nav != 1 && exr != 1 && cre == 1) {
		printf("creating archive...\n");
		if (!cre_folders) {
			printf("-h for help.");
			free(file);
			return 1;
		}
		if (strchr(cre_folders, ',')) {
		char* folders = strtok(cre_folders, ",");
		while (folders != NULL) {
			folder_file_handler(&fs, folders, fs.movement_parent);
			folders = strtok(NULL, ",");
		}
		} else {
			fs.movement_parent = 0;
			folder_file_handler(&fs, cre_folders, fs.movement_parent);
		}
		free(cre_folders);

		if (gzip == 1) {
			int size = strlen("gzip ") + strlen(file) + 1;
			char *full_command = malloc(size);
			snprintf(full_command,  size, "gzip %s", file);
			system(full_command);
			free(full_command);
		} 
		else if (xz == 1) {
			int size = strlen("xz ") + strlen(file) + 1;
			char *full_command = malloc(size);
			snprintf(full_command,  size, "xz %s", file);
			system(full_command);
			free(full_command);

		} 
		else if (lz4 == 1) {
			int size = strlen("lz4 ") + strlen(file) + 1;
			char *full_command = malloc(size);
			snprintf(full_command,  size, "lz4 %s", file);
			system(full_command);
			free(full_command);

		}
	
	}

	fclose(fs.img);
	return 0;
}
