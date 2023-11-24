#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
//#include "mkdir.h"
//#include "globals.h"

typedef struct __attribute__((packed)) BPB {
        char BS_jmpBoot[3];
        char BS_OEMName[8];
        uint16_t BPB_BytsPerSec;
        uint8_t BPB_SecPerClus;
        uint16_t BPB_RsvdSecCnt;
        uint8_t BPB_NumFATs;
        uint16_t BPB_RootEntCnt;
        uint16_t BPB_TotSec16;
        uint8_t BPB_Media;
        uint16_t BPB_FATSz16;
        uint32_t BPB_FATSz32;
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
void process_directory_entries(int fat32_fd, uint32_t cluster_num, bpb_t bpb);
void print_boot_sector_info(bpb_t bpb);
void dbg_print_dentry(dentry_t *dentry);
//void mkdir(bpb_t bpb, const char* new);
bpb_t mount_fat32(int img_fd);
uint32_t convert_offset_to_clus_num_in_fat_region(uint32_t offset);
uint32_t convert_clus_num_to_offset_in_data_region(uint32_t clus_num, bpb_t bpb);
uint32_t allocate_cluster(int fd, bpb_t bpb);
void extend_cluster_chain(int fd, uint32_t final_clus_num, bpb_t bpb);
dentry_t *encode_dir_entry(int fat32_fd, uint32_t offset);
void write_dir_entry(int fd, dentry_t *dentry, uint32_t offset);
void append_dir_entry(int fd, dentry_t *new_dentry, uint32_t clus_num, bpb_t bpb);
bool is_end_of_file_or_bad_cluster(uint32_t clus_num);
uint32_t convert_clus_num_to_offset_in_fat_region(uint32_t clus_num);
//ADD FUNCTION DECLARATIONS HERE

// other data structure, global variables, etc. define them in need.
// e.g., 
// the opened fat32.img file
// the current working directory
char current_path[256] = "/";//UPDATE ON cd
// the opened files
// other data structures and global variables you need

//main function (loops) (all the functionality is called or written here)
void main_process(int img_fd, const char* img_path, bpb_t bpb) {
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
        else if(strcmp(tokens->items[0], "mkdir") == 0 && tokens->size > 2)
            printf("mkdir command does not take more than two arguments\n");
        else if (strcmp(tokens->items[0], "mkdir") == 0)
            mkdir(img_fd, img_path, bpb, tokens->items[1]);
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

    uint32_t offset = 0;
    uint32_t curr_clus_num = 3;
    uint32_t next_clus_num = 0;
    uint32_t BPB_SecPerClus = 1;
    uint32_t BPB_FATSz32 = 1009;
    uint32_t max_clus_num = BPB_FATSz32 / BPB_SecPerClus;
    uint32_t min_clus_num = 2;

    while (curr_clus_num >= min_clus_num && curr_clus_num <= max_clus_num) {
        // if the cluster number is not in the range, this cluster is:
        // 1. reserved cluster
        // 2. end of the file
        // 3. bad cluster.
        // No matter which kind of number, we can consider it is the end of a file.
        if (is_end_of_file_or_bad_cluster(curr_clus_num)) {
            printf("This cluster is the end of a file or a bad cluster: %u\n", curr_clus_num);
            break; // Exit the loop if it's the end of a file or a bad cluster.
        }

	process_directory_entries(img_fd, curr_clus_num, bpb);

        offset = convert_clus_num_to_offset_in_fat_region(curr_clus_num);
        pread(img_fd, &next_clus_num, sizeof(uint32_t), offset);
        printf("current cluster number: %u, next cluster number: %u\n", \
                curr_clus_num, next_clus_num);
        curr_clus_num = next_clus_num;
    }

    // 3. main procees
    main_process(img_fd, img_path, bpb);

    // 4. close all opened files

    // 5. close the fat32.img
    close(img_fd);

    return 0;
}


//void mkdir(bpb_t bpb, const char* new_dir_name) {
    //NA
//}

void dbg_print_dentry(dentry_t *dentry) {
    if (dentry == NULL) {
        return;
    }

    // Combine DIR_FstClusHI and DIR_FstClusLO to get the correct cluster number
    uint32_t firstCluster = ((uint32_t)dentry->DIR_FstClusHI << 16) | (uint32_t)dentry->DIR_FstClusLO;

    printf("DIR_Name: %s\n", dentry->DIR_Name);
    printf("DIR_Attr: 0x%x\n", dentry->DIR_Attr);
    printf("First Cluster Number: %u\n", firstCluster);
    printf("DIR_FileSize: %u\n", dentry->DIR_FileSize);
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

// Revised function to read and process directory entries
void process_directory_entries(int fat32_fd, uint32_t cluster_num, bpb_t bpb) {
    uint32_t sectorSize = bpb.BPB_BytsPerSec;
    uint32_t clusterSize = bpb.BPB_SecPerClus * sectorSize;
    uint32_t dataRegionOffset = ((cluster_num - 2) * clusterSize) +
                                (bpb.BPB_NumFATs * bpb.BPB_FATSz32 * sectorSize) +
                                (bpb.BPB_RsvdSecCnt * sectorSize);

    for (uint32_t i = 0; i < clusterSize; i += sizeof(dentry_t)) {
        dentry_t dentry;
        ssize_t rd_bytes = pread(fat32_fd, &dentry, sizeof(dentry_t), dataRegionOffset + i);

        if (rd_bytes != sizeof(dentry_t)) {
		//Handle error
		break;
	}

	//Check if the entry is a valid file or directory
	if ((dentry.DIR_Attr & 0x10) == 0x10 || (dentry.DIR_Attr & 0x20) == 0x20) {
            dbg_print_dentry(&dentry);
        }
    }
}

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

// the offset to the beginning of the file.
uint32_t convert_offset_to_clus_num_in_fat_region(uint32_t offset) {
    uint32_t fat_region_offset = 0x4000;
    return (offset - fat_region_offset)/4;
}

uint32_t convert_clus_num_to_offset_in_fat_region(uint32_t clus_num) {
    uint32_t fat_region_offset = 0x4000;
    return fat_region_offset + clus_num * 4;
}

uint32_t convert_clus_num_to_offset_in_data_region(uint32_t clus_num, bpb_t bpb) {
    uint32_t data_region_offset = (clus_num - 2) * bpb.BPB_SecPerClus * bpb.BPB_BytsPerSec;
    return data_region_offset;
}

// returns the free cluster number
uint32_t allocate_cluster(int fd, bpb_t bpb) {
    /*uint32_t fat_region = 0x4000;
    uint32_t min_clus_num = 2;
    uint32_t max_clus_num = 1009;
    uint32_t clus_clus_num = min_clus_num;
    uint32_t next_clus_num = 0xffffffff;
    
    while (clus_clus_num != 0) {
	uint32_t offset = convert_clus_num_to_offset_in_fat_region(clus_clus_num);
        pread(fd, &next_clus_num, sizeof(uint32_t), offset);
        if (next_clus_num == 0) {
            // current cluster number is free.
            return clus_clus_num;
        } else {
            // check the next cluster number
            ++ clus_clus_num;
        }
    }
    // no free cluster found.
    return 0;*/
    uint32_t fat_region_offset = bpb.BPB_RsvdSecCnt * bpb.BPB_BytsPerSec;
    uint32_t fat_entry_size = 4; // Each FAT entry is 4 bytes for FAT32

    for (uint32_t i = 2; i < bpb.BPB_FATSz32; i++) {
        uint32_t offset = fat_region_offset + i * fat_entry_size;
        uint32_t fat_entry;
        ssize_t read_bytes = pread(fd, &fat_entry, sizeof(uint32_t), offset);

        if (read_bytes != sizeof(uint32_t)) {
            // Handle error reading FAT entry
            return 0;
        }

        if (fat_entry == 0) {
            // Found a free cluster
            return i;
        }
    }

    // No free cluster found
    return 0;
}

// add a new cluster to the cluster chain.
void extend_cluster_chain(int fd, uint32_t final_clus_num, bpb_t bpb) {
    // get a free cluster number
    uint32_t new_clus_num = allocate_cluster(fd, bpb);

    // set the new custer as the final cluster of a file.
    uint32_t offset = convert_clus_num_to_offset_in_fat_region(new_clus_num);
    uint32_t end_of_file = 0xffffffff;
    pwrite(fd, &end_of_file, sizeof(uint32_t), offset);

    // update the cluster chain by updating the old final cluster.
    offset = convert_clus_num_to_offset_in_fat_region(final_clus_num);
    pwrite(fd, &new_clus_num, sizeof(uint32_t), offset);
}

// This function returns one directory entry. (PESUDOCODE)
dentry_t *encode_dir_entry(int fat32_fd, uint32_t offset) {
    dentry_t *dentry = (dentry_t*)malloc(sizeof(dentry_t));
    ssize_t rd_bytes = pread(fat32_fd, (void*)dentry, sizeof(dentry_t), offset);

    if (rd_bytes != sizeof(dentry_t)) {
        // Handle error: The read data size doesn't match the size of a directory entry.
        free(dentry);
        return NULL;
    }

    // Check if the attributes indicate that it's a directory entry
    if ((dentry->DIR_Attr & 0x08) != 0) {
        // This is a volume label, not a directory entry
        free(dentry);
        return NULL;
    }

    return dentry;
}

// This is just an example and pseudocode. The real logic may different from
// what is shown here.
// This function writes the dentry to the offset.
void write_dir_entry(int fd, dentry_t *dentry, uint32_t offset) {
    uint32_t wr_bytes = pwrite(fd, dentry, sizeof(dentry_t), offset);
    // omitted: check wr_bytes == sizeof(dentry_t)

    return dentry;
}

// This is just an example and pseudocode. The real logic may different from
// what is shown here.
// This function appends a dentry to the cluster in the data region.
void append_dir_entry(int img_fd, dentry_t *new_dentry, uint32_t clus_num, bpb_t bpb) {
    uint32_t curr_clus_num = clus_num;
    uint32_t next_clus_num = 0;
    //uint32_t fat_offset = convert_clus_num_to_offset_in_fat_region(curr_clus_num);
    //uint32_t data_offset = convert_clus_num_to_offset_in_data_region(curr_clus_num);
    uint32_t data_region_offset = convert_clus_num_to_offset_in_data_region(curr_clus_num, bpb);
    uint32_t data_region_size = bpb.BPB_SecPerClus * bpb.BPB_BytsPerSec;

    while (curr_clus_num < 0x0FFFFFF8) {
        // Check if the current cluster is full
        if (data_region_offset >= data_region_size) {
            // Allocate a new cluster and extend the cluster chain
            uint32_t new_cluster = allocate_cluster(img_fd, bpb);
            if (new_cluster == 0) {
                // Handle error: No free cluster available
                return;
            }

            // Update the FAT to link the new cluster
            uint32_t fat_region_offset = bpb.BPB_RsvdSecCnt * bpb.BPB_BytsPerSec;
            uint32_t fat_entry_offset = fat_region_offset + curr_clus_num * 4;
            pwrite(img_fd, &new_cluster, sizeof(uint32_t), fat_entry_offset);

            // Update next cluster and reset data_region_offset
            curr_clus_num = new_cluster;
            data_region_offset = convert_clus_num_to_offset_in_data_region(curr_clus_num, bpb);
        }

        // Check if the directory entry is free in the current cluster
        dentry_t existing_dentry;
        ssize_t read_bytes = pread(img_fd, &existing_dentry, sizeof(dentry_t), data_region_offset);

        if (read_bytes != sizeof(dentry_t)) {
            // Handle error reading directory entry
            return;
        }

        if (existing_dentry.DIR_Name[0] == 0xE5 || existing_dentry.DIR_Name[0] == 0x00) {
            // Found a free or deleted directory entry, write the new entry
            pwrite(img_fd, new_dentry, sizeof(dentry_t), data_region_offset);
            return;
        }

        // Move to the next directory entry in the cluster
        data_region_offset += sizeof(dentry_t);
    }

    // Handle error: No available directory entry in the cluster chain
    return;

}

bool is_end_of_file_or_bad_cluster(uint32_t clus_num) {
    // Define the values that indicate the end of a file and bad clusters in FAT32.
    uint32_t fat32_end_of_file = 0x0FFFFFFF;
    uint32_t fat32_bad_cluster_min = 0x0FFFFFF8;
    uint32_t fat32_bad_cluster_max = 0x0FFFFFFF;

    // Check if the cluster number falls within the range of bad clusters or is the end of the file.
    if ((clus_num >= fat32_bad_cluster_min && clus_num <= fat32_bad_cluster_max) || clus_num == fat32_end_of_file) {
        return true;
    }

    return false;
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
