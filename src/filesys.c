#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>

typedef struct __attribute__((packed)) BPB {
    char BS_jmpBoot[3]; // unnecessary to parse it, use a char array to hold space.
    char BS_OEMName[8]; // unnecessary to parse it, use a char array to hold space.
    uint16_t BPB_BytsPerSec; // offset 11, size 2.
    uint8_t BPB_SecPerClus;
	uint16_t BPB_RsvdSecCnt;
	uint8_t BPB_NumFATs;
	uint16_t BPB_RootEntCnt;
	uint16_t BPB_TotSec16;
	uint8_t BPB_Media;
	uint16_t BPB_FATSz16;
	uint16_t BPB_SecPerTrk;
	uint16_t BPB_NumHeads;
	uint32_t BPB_HiddSec;
	uint32_t BPB_TotSec32;
} bpb_t;

typedef struct __attribute__((packed)) directory_entry {
    char DIR_Name[11];
    uint8_t DIR_Attr;
    char padding_1[8]; // DIR_NTRes, DIR_CrtTimeTenth, DIR_CrtTime, DIR_CrtDate, 
                       // DIR_LstAccDate. Since these fields are not used in
                       // Project 3, just define as a placeholder.
    uint16_t DIR_FstClusHI;
    char padding_2[4]; // DIR_WrtTime, DIR_WrtDate
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} dentry_t;


typedef struct {
    char ** items;
    size_t size;
} tokenlist;

char * get_input(void);
tokenlist * get_tokens(char *input);
tokenlist * new_tokenlist(void);
void add_token(tokenlist *tokens, char *item);
void free_tokens(tokenlist *tokens);
void print_boot_sector_info(bpb_t bpb);
//ADD FUNCTION DECLARATIONS HERE

// other data structure, global variables, etc. define them in need.
// e.g., 
// the opened fat32.img file
// the current working directory
char current_path[256] = "/";//UPDATE ON cd
// the opened files
// other data structures and global variables you need

//mounts the fat23 image file
bpb_t mount_fat32(int img_fd) {
    bpb_t bpb;
    size_t rd_bytes = pread(img_fd, &bpb, sizeof(bpb_t), 0);
    if (rd_bytes == -1) {
        perror("pread failed");
        close(img_fd);
        exit(EXIT_FAILURE);
    }
    // check if rd_bytes == sizeof(BPB_BytsPerSec)
    if (rd_bytes != sizeof(bpb_t)) {
        printf("request %zu bytes, but read %zu bytes\n", sizeof(bpb_t), rd_bytes);
        close(img_fd);
        exit(EXIT_FAILURE);
    }
    //printf("BPB_BytsPerSec: %u\n", bpb.BPB_BytsPerSec);

    return bpb;
}

//main function (loops) (all the functionality is called or written here)
void main_process(const char* img_path, bpb_t bpb) {
    while (1) {
        // 0. print prompt and current working directory
        printf("%s%s>",img_path, current_path);
        // 1. get cmd from input.
        // you can use the parser provided in Project1
        char *input = get_input();
        tokenlist *tokens = get_tokens(input);
        if(tokens->size <= 0)
			continue;

        // 2. if cmd is "exit" break;
        if(strcmp(tokens->items[0], "exit") == 0 && tokens->size > 1)
            printf("exit command does not take any arguments\n");
        else if(strcmp(tokens->items[0], "exit") == 0)
        {
            free_tokens(tokens);
            break;
        }
        else if(strcmp(tokens->items[0], "info") == 0 && tokens->size > 1)
            printf("info command does not take any arguments\n");
        else if (strcmp(tokens->items[0], "info") == 0)
            print_boot_sector_info(bpb);
        // else if cmd is "cd" process_cd();
        // else if cmd is "ls" process_ls();
        // ...
        
        free_tokens(tokens);
    }
}

//main function from which to execute initalizations, exits, and main loop
int main(int argc, char const *argv[])
{
    // 0. check provided arguments (./filesys fat32.img)
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <fat32_image_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    // 1. open the fat32.img
    const char *img_path = argv[1];
    int img_fd = open(img_path, O_RDONLY);
    if (img_fd == -1) {
        perror("open failed");
        return EXIT_FAILURE;
    }

    // 2. mount the fat32.img
    bpb_t bpb = mount_fat32(img_fd);

    // 3. main procees
    main_process(img_path, bpb);


    // 4. close all opened files

    // 5. close the fat32.img
    close(img_fd);

    return 0;
}

void print_boot_sector_info(bpb_t bpb) {
    printf("Position of Root Cluster: %u\n", bpb.BPB_RootEntCnt);
    printf("Bytes Per Sector: %u\n", bpb.BPB_BytsPerSec);
    printf("Sectors Per Cluster: %u\n", bpb.BPB_SecPerClus);
    // Calculate and print total number of clusters in data region
    uint32_t totalClusters = (bpb.BPB_TotSec32 - bpb.BPB_RsvdSecCnt) / bpb.BPB_SecPerClus;
    printf("Total # of Clusters in Data Region: %u\n", totalClusters);
    printf("# of Entries in One FAT: %u\n", bpb.BPB_NumFATs);
    uint32_t imageSize = bpb.BPB_TotSec32 * bpb.BPB_BytsPerSec;
    printf("Size of Image (in bytes): %u\n", imageSize);
}

//GETS THE INPUT FROM THE COMMAND LINE
char *get_input(void) {
	char *buffer = NULL;
	int bufsize = 0;
	char line[5];
	while (fgets(line, 5, stdin) != NULL)
	{
		int addby = 0;
		char *newln = strchr(line, '\n');
		if (newln != NULL)
			addby = newln - line;
		else
			addby = 5 - 1;
		buffer = (char *)realloc(buffer, bufsize + addby);
		memcpy(&buffer[bufsize], line, addby);
		bufsize += addby;
		if (newln != NULL)
			break;
	}
	buffer = (char *)realloc(buffer, bufsize + 1);
	buffer[bufsize] = 0;
	return buffer;
}

//MAKES AN EMPTY TOKENLIST
tokenlist *new_tokenlist(void) {
	tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
	tokens->size = 0;
	tokens->items = (char **)malloc(sizeof(char *));
	tokens->items[0] = NULL; /* make NULL terminated */
	return tokens;
}

//ADDS A TOKEN WITH SPECIFIED ITEM TO THE TOKENLIST PROVIDED 
void add_token(tokenlist *tokens, char *item) {
	int i = tokens->size;

	tokens->items = (char **)realloc(tokens->items, (i + 2) * sizeof(char *));
	tokens->items[i] = (char *)malloc(strlen(item) + 1);
	tokens->items[i + 1] = NULL;
	strcpy(tokens->items[i], item);

	tokens->size += 1;
}

//GETS THE TOKENS FROM AN INPUT (PARSES)
tokenlist *get_tokens(char *input) {
	char *buf = (char *)malloc(strlen(input) + 1);
	strcpy(buf, input);
	tokenlist *tokens = new_tokenlist();
	char *tok = strtok(buf, " ");
	while (tok != NULL)
	{
		add_token(tokens, tok);
		tok = strtok(NULL, " ");
	}
	free(buf);
	return tokens;
}

//FREES THE TOKENS
void free_tokens(tokenlist *tokens) {
	for (int i = 0; i < tokens->size; i++)
		free(tokens->items[i]);
	free(tokens->items);
	free(tokens);
}