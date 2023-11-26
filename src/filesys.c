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
        uint32_t BPB_RootClus;
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
bpb_t mount_fat32(int img_fd);
bool is_end_of_file_or_bad_cluster(uint32_t clus_num);
uint32_t convert_clus_num_to_offset_in_fat_region(uint32_t clus_num);
uint32_t convert_offset_to_clus_num_in_fat_region(uint32_t offset);
uint32_t convert_clus_num_to_offset_in_data_region(uint32_t clus_num);
void write_dir_entry(int fd, dentry_t *dentry, uint32_t offset);
void append_dir_entry(int fd, dentry_t *new_dentry, uint32_t clus_num);
uint32_t alloca_cluster(int fd);
void extend_cluster_chain(int fd, uint32_t final_clus_num);
bool is_valid_path(int fd_img, bpb_t bpb, const char* path);
uint32_t directory_location(int fd_img, bpb_t bpb);
bool is_directory(int fd_img, bpb_t bpb, const char* dir_name);
bool is_file(int fd_img, bpb_t bpb, const char* file_name);
bool is_8_3_format(const char* name);
void new_directory(int fd_img, bpb_t bpb, const char* dir_name);
void new_file(int fd_img, bpb_t bpb, const char* file_name);
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
        else if(strcmp(tokens->items[0], "cd") == 0 && tokens->size > 2)
            printf("cd command does not take more than two arguments\n");
        else if (strcmp(tokens->items[0], "cd") == 0)
            if(!is_valid_path(fd_img,bpb,tokens->items[1]))
                printf("%s does not exist\n", tokens->items[1]);
        else if(strcmp(tokens->items[0], "mkdir") == 0 && tokens->size > 2)
            printf("mkdir command does not take more than two arguments\n");
        else if (strcmp(tokens->items[0], "mkdir") == 0)
            if(!is_directory(fd_img, bpb, tokens->items[1]))
                new_directory(img_fd, img_path, bpb, tokens->items[1]);
        else if(strcmp(tokens->items[0], "creat") == 0 && tokens->size > 2)
            printf("creat command does not take more than two arguments\n");
        else if (strcmp(tokens->items[0], "creat") == 0)
            if(!is_file(fd_img, bpb, tokens->items[1]))
                new_file(img_fd, img_path, bpb, tokens->items[1]);
        // else if cmd is ...
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

bool is_valid_path(int fd_img, bpb_t bpb, const char* path) {
    char original_path[256];
    strncpy(original_path, current_path, sizeof(original_path)); // Save the current path

    char temp_path[256];
    if (path[0] == '/') {
        // Absolute path
        strncpy(temp_path, path, sizeof(temp_path));
    } else {
        // Relative path
        snprintf(temp_path, sizeof(temp_path), "%s%s", current_path, path);
    }

    // Normalize the path (handle '.' and '..')
    char *token;
    char *tokens[256];
    int token_count = 0;
    token = strtok(temp_path, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // Current directory, do nothing
        } else if (strcmp(token, "..") == 0) {
            // Parent directory, go one level up
            if (token_count > 0) token_count--;
        } else {
            tokens[token_count++] = token;
        }
        token = strtok(NULL, "/");
    }

    // Reconstruct the absolute path
    char absolute_path[256] = "/";
    for (int i = 0; i < token_count; i++) {
        if (i > 0) strcat(absolute_path, "/");
        strcat(absolute_path, tokens[i]);
    }

    // Temporarily update current_path
    strncpy(current_path, absolute_path, sizeof(current_path));

    // Check if it's a valid directory
    uint32_t cluster_num = directory_location(fd_img, bpb);
    if (cluster_num == 0) {
        strncpy(current_path, original_path, sizeof(current_path)); // Restore original path
        return false; // Not a valid directory
    }

    // Keep the updated path in current_path as it's valid
    return true;
}

uint32_t directory_location(int fd_img, bpb_t bpb) {
    uint32_t clusterNum = bpb.BPB_RootClus;
    uint32_t sectorSize = bpb.BPB_BytsPerSec;
    uint32_t clusterSize = bpb.BPB_SecPerClus * sectorSize;
    uint32_t nextClusterNum;
    char buffer[clusterSize];
    dentry_t *dirEntry;

    char *token;
    char *full_path_copy = strdup(current_path);
    token = strtok(full_path_copy, "/");
    while (token != NULL) {
        bool found = false;
        do {
            // Read the current directory's entries
            uint32_t dataRegionOffset = 0x100400 + (clusterNum - 2) * clusterSize;
            ssize_t bytesRead = pread(fd_img, buffer, clusterSize, dataRegionOffset);

            if (bytesRead <= 0) {
                free(full_path_copy);
                return 0;
            }

            for (uint32_t i = 0; i < bytesRead; i += sizeof(dentry_t)) {
                dirEntry = (dentry_t *)(buffer + i);
                if (dirEntry->DIR_Name[0] == 0x00) { // End of directory entries
                    found = false;
                    break;
                }
                if (strncmp(dirEntry->DIR_Name, token, 11) == 0 && (dirEntry->DIR_Attr & 0x10)) {
                    found = true;
                    clusterNum = ((uint32_t)dirEntry->DIR_FstClusHI << 16) | (uint32_t)dirEntry->DIR_FstClusLO;
                    break;
                }
            }

            if (!found) {
                // Get next cluster number from FAT
                uint32_t fatOffset = convert_clus_num_to_offset_in_fat_region(clusterNum);
                pread(fd_img, &nextClusterNum, sizeof(uint32_t), fatOffset);
                clusterNum = nextClusterNum;
            }
        } while (!found && !is_end_of_file_or_bad_cluster(clusterNum));

        if (!found) {
            free(full_path_copy);
            return 0;
        }

        token = strtok(NULL, "/");
    }

    free(full_path_copy);
    return clusterNum;
}

bool is_directory(int fd_img, bpb_t bpb, const char* dir_name) {
    char upper_dir_name[12];
    strncpy(upper_dir_name, dir_name, 11);
    upper_dir_name[11] = '\0'; // Ensure null termination
    for (int i = 0; upper_dir_name[i] != '\0'; i++) {
        upper_dir_name[i] = toupper((unsigned char)upper_dir_name[i]);
    }

    if (!is_8_3_format(upper_dir_name)) {
        printf("%s is not in fat32 8.3 format", dir_name);
        return true; // Return true if not in 8.3 format
    }

    uint32_t clusterNum = directory_location(fd_img, bpb);
    uint32_t nextClusterNum;
    uint32_t dirEntrySize = sizeof(dentry_t);
    uint32_t bufferSize = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
    char buffer[bufferSize];
    dentry_t *dirEntry;
    bool found = false;

    do {
        uint32_t dataRegionOffset = 0x100400 + (clusterNum - 2) * bufferSize;
        ssize_t bytesRead = pread(fd_img, buffer, bufferSize, dataRegionOffset);

        if (bytesRead <= 0) {
            perror("pread failed");
            printf("data region could not be read\n");
            return true;
        }

        for (uint32_t i = 0; i < bytesRead; i += dirEntrySize) {
            dirEntry = (dentry_t *)(buffer + i);

            // End of directory marker
            if (dirEntry->DIR_Name[0] == 0x00) {
                return false;
            }

            // Skip deleted entries and check for directory name match
            if (dirEntry->DIR_Name[0] != 0xE5 &&
                strncmp(dirEntry->DIR_Name, dir_name, 11) == 0 && 
                (dirEntry->DIR_Attr & 0x10)) {
                printf("directory exists\n");
                return true;
            }
        }

        // Get next cluster number from FAT
        uint32_t fatOffset = convert_clus_num_to_offset_in_fat_region(clusterNum);
        pread(fd_img, &nextClusterNum, sizeof(uint32_t), fatOffset);
        clusterNum = nextClusterNum;

    } while (!is_end_of_file_or_bad_cluster(clusterNum));

    return false;
}

bool is_file(int fd_img, bpb_t bpb, const char* file_name) {
    char upper_file_name[12];
    strncpy(upper_file_name, file_name, 11);
    upper_file_name[11] = '\0'; // Ensure null termination
    for (int i = 0; upper_file_name[i] != '\0'; i++) {
        upper_file_name[i] = toupper((unsigned char)upper_file_name[i]);
    }

    if (!is_8_3_format(upper_file_name)) {
        printf("%s is not in fat32 8.3 format", dir_name);
        return true; // Return true if not in 8.3 format
    }

    uint32_t clusterNum = directory_location(fd_img, bpb);
    uint32_t nextClusterNum;
    uint32_t dirEntrySize = sizeof(dentry_t);
    uint32_t bufferSize = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
    char buffer[bufferSize];
    dentry_t *dirEntry;
    bool found = false;

    do {
        uint32_t dataRegionOffset = 0x100400 + (clusterNum - 2) * bufferSize;
        ssize_t bytesRead = pread(fd_img, buffer, bufferSize, dataRegionOffset);

        if (bytesRead <= 0) {
            perror("pread failed");
            printf("data region could not be read\n");
            return true;
        }

        for (uint32_t i = 0; i < bytesRead; i += dirEntrySize) {
            dirEntry = (dentry_t *)(buffer + i);

            // End of directory marker
            if (dirEntry->DIR_Name[0] == 0x00) {
                return false;
            }

            // Skip deleted entries and check for file name match
            if (dirEntry->DIR_Name[0] != 0xE5 &&
                strncmp(dirEntry->DIR_Name, file_name, 11) == 0 && 
                (dirEntry->DIR_Attr & 0x20)) {
                printf("file exists\n");
                return true;
            }
        }

        // Get next cluster number from FAT
        uint32_t fatOffset = convert_clus_num_to_offset_in_fat_region(clusterNum);
        pread(fd_img, &nextClusterNum, sizeof(uint32_t), fatOffset);
        clusterNum = nextClusterNum;

    } while (!is_end_of_file_or_bad_cluster(clusterNum));
    return false;
}

bool is_8_3_format(const char* name) {
    int name_len = 0;
    bool dot_encountered = false;

    for (int i = 0; name[i] != '\0'; i++) {
        if (name[i] == '.') {
            if (dot_encountered || i > 8) return false; // More than one dot or name part too long
            dot_encountered = true;
            name_len = 0; // Reset length for extension part
        } else {
            name_len++;
            if ((dot_encountered && name_len > 3) || (!dot_encountered && name_len > 8)) {
                return false; // Extension or name part too long
            }
        }
    }
    return true;
}

void new_directory(int fd_img, bpb_t bpb, const char* dir_name) {
    // Convert dir_name to FAT32 8.3 format
    // Assuming a function that does this conversion

    // Allocate a cluster for the new directory
    uint32_t free_cluster = alloc_cluster(fd_img);
    if (free_cluster == 0) {
        printf("Failed to allocate a cluster for the new directory\n");
        return;
    }

    // Create a directory entry for the new directory
    dentry_t new_dir_entry = {0};
    strncpy(new_dir_entry.DIR_Name, dir_name, 11); // Copy dir_name to DIR_Name
    new_dir_entry.DIR_Attr = 0x10; // Directory attribute
    new_dir_entry.DIR_FstClusHI = (free_cluster >> 16) & 0xFFFF;
    new_dir_entry.DIR_FstClusLO = free_cluster & 0xFFFF;
    new_dir_entry.DIR_FileSize = 0; // A directory's size is 0

    // Find the location of the current directory and append the new directory entry
    uint32_t current_dir_cluster = directory_location(fd_img, bpb);
    append_dir_entry(fd_img, &new_dir_entry, current_dir_cluster, bpb);
}

void new_file(int fd_img, bpb_t bpb, const char* file_name) {
    // Convert file_name to FAT32 8.3 format
    // Assuming a function that does this conversion

    // Allocate a cluster for the new file
    uint32_t free_cluster = alloc_cluster(fd_img);
    if (free_cluster == 0) {
        printf("Failed to allocate a cluster for the new file\n");
        return;
    }

    // Create a directory entry for the new file
    dentry_t new_file_entry = {0};
    strncpy(new_file_entry.DIR_Name, file_name, 11); // Copy file_name to DIR_Name
    new_file_entry.DIR_Attr = 0x20; // File attribute
    new_file_entry.DIR_FstClusHI = (free_cluster >> 16) & 0xFFFF;
    new_file_entry.DIR_FstClusLO = free_cluster & 0xFFFF;
    new_file_entry.DIR_FileSize = 0; // Initial size is 0

    // Find the location of the current directory and append the new file entry
    uint32_t current_dir_cluster = directory_location(fd_img, bpb);
    append_dir_entry(fd_img, &new_file_entry, current_dir_cluster, bpb);
}

void write_dir_entry(int fd, dentry_t *dentry, uint32_t offset) {
    ssize_t wr_bytes = pwrite(fd, dentry, sizeof(dentry_t), offset);
    if (wr_bytes != sizeof(dentry_t)) {
        printf("could not write directory entry\n");
    }
}

void append_dir_entry(int fd, dentry_t *new_dentry, uint32_t clus_num, bpb_t bpb) {
    uint32_t curr_clus_num = clus_num;
    uint32_t next_clus_num = 0;
    uint32_t sectorSize = bpb.BPB_BytsPerSec;
    uint32_t clusterSize = bpb.BPB_SecPerClus * sectorSize;
    uint32_t bytesProcessed = 0;

    while (curr_clus_num != 0xFFFFFFFF) {
        uint32_t data_offset = convert_clus_num_to_offset_in_data_region(curr_clus_num);

        while (bytesProcessed < clusterSize) {
            dentry_t dentry;
            ssize_t rd_bytes = pread(fd, &dentry, sizeof(dentry_t), data_offset + bytesProcessed);
            if (rd_bytes != sizeof(dentry_t)) {
                printf("Failed to read directory entry\n");
                return;
            }

            if (dentry.DIR_Name[0] == 0x00 || dentry.DIR_Name[0] == 0xE5) {
                write_dir_entry(fd, new_dentry, data_offset + bytesProcessed);
                return;
            }

            bytesProcessed += sizeof(dentry_t);
        }

        // Reset for the next cluster
        bytesProcessed = 0;
        uint32_t fat_offset = convert_clus_num_to_offset_in_fat_region(curr_clus_num);
        pread(fd, &next_clus_num, sizeof(uint32_t), fat_offset);

        if (next_clus_num == 0xFFFFFFFF) {
            next_clus_num = alloc_cluster(fd);
            pwrite(fd, &next_clus_num, sizeof(uint32_t), fat_offset);
            curr_clus_num = next_clus_num;
        } else {
            curr_clus_num = next_clus_num;
        }
    }
}

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

uint32_t convert_offset_to_clus_num_in_fat_region(uint32_t offset) {
    uint32_t fat_region_offset = 0x4000;
    return (offset - fat_region_offset)/4;
}

uint32_t convert_clus_num_to_offset_in_fat_region(uint32_t clus_num) {
    uint32_t fat_region_offset = 0x4000;
    return fat_region_offset + clus_num * 4;
}

uint32_t convert_clus_num_to_offset_in_data_region(uint32_t clus_num) {
    uint32_t clus_size = 512;
    uint32_t data_region_offset = 0x100400;
    return data_region_offset + (clus_num - 2) * clus_size;
}

uint32_t alloca_cluster(int fd) {
    uint32_t min_clus_num = 2;
    uint32_t max_clus_num = 1009;
    uint32_t clus_clus_num = min_clus_num;
    uint32_t next_clus_num = 0xffffffff;
    
    while (clus_clus_num <= max_clus_num) {
        uint32_t offset = convert_clus_num_to_offset_in_fat_region(clus_clus_num);
        pread(fd, &next_clus_num, sizeof(uint32_t), offset);
        if (next_clus_num == 0) {
            uint32_t end_of_chain = 0xFFFFFFFF;
            pwrite(fd, &end_of_chain, sizeof(uint32_t), offset);  // Mark the cluster as used
            return clus_clus_num;
        } else {
            ++clus_clus_num;
        }
    }
    printf("No free cluster found\n");
    return 0; // No free cluster found
}

void extend_cluster_chain(int fd, uint32_t *current_clus_num_ptr, dentry_t *dentry_ptr) {
    // Allocate a new cluster
    uint32_t new_clus_num = alloca_cluster(fd);
    if (new_clus_num == 0) {
        // Handle the error: No free cluster available
        return;
    }

    if (*current_clus_num_ptr == 0) {
        // The file or directory has no clusters yet. Set the first cluster.
        *current_clus_num_ptr = new_clus_num;
        dentry_ptr->DIR_FstClusHI = (new_clus_num >> 16) & 0xFFFF;
        dentry_ptr->DIR_FstClusLO = new_clus_num & 0xFFFF;

        // Write the updated directory entry back to the disk
        // Assuming a function write_dentry() that writes the directory entry at the correct location
        write_dentry(fd, dentry_ptr);
    } else {
        // The file or directory already has clusters. Extend the chain.
        uint32_t final_clus_num = *current_clus_num_ptr;
        uint32_t final_offset = convert_clus_num_to_offset_in_fat_region(final_clus_num);
        pwrite(fd, &new_clus_num, sizeof(uint32_t), final_offset);

        // Mark the new cluster as the end of the chain
        uint32_t new_offset = convert_clus_num_to_offset_in_fat_region(new_clus_num);
        uint32_t end_of_file = 0xFFFFFFFF;
        pwrite(fd, &end_of_file, sizeof(uint32_t), new_offset);
    }
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
