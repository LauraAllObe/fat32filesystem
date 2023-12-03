#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>

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
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint32_t BPB_FATSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint16_t BPB_FSInfo;
    uint16_t BPB_BkBootSec;
    char BPB_Reserved[12];
} bpb_t;


typedef struct __attribute__((packed)) directory_entry {
    char DIR_Name[11];
    uint8_t DIR_Attr;
    char padding_1[8];
    uint16_t DIR_FstClusHI;
    char padding_2[4];
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
void dbg_print_dentry(dentry_t *dentry);
bpb_t mount_fat32(int img_fd);
bool is_end_of_file_or_bad_cluster(uint32_t clus_num);
uint32_t convert_clus_num_to_offset_in_fat_region(uint32_t clus_num, bpb_t bpb);
uint32_t convert_offset_to_clus_num_in_fat_region(uint32_t offset, bpb_t bpb);
uint32_t convert_clus_num_to_offset_in_data_region(uint32_t clus_num, bpb_t bpb);
void write_dir_entry(int fd, dentry_t *dentry, uint32_t offset);
void append_dir_entry(int fd, dentry_t *new_dentry, uint32_t clus_num, bpb_t bpb);
uint32_t alloca_cluster(int fd, bpb_t bpb);
void clear_cluster(int fd, uint32_t cluster_num, bpb_t bpb);
void extend_cluster_chain(int fd, uint32_t *current_clus_num_ptr, dentry_t *dentry_ptr, bpb_t bpb);
bool is_valid_path(int fd_img, bpb_t bpb, const char* path);
uint32_t directory_location(int fd_img, bpb_t bpb);
bool is_directory(int fd_img, bpb_t bpb, const char* dir_name);
bool is_file(int fd_img, bpb_t bpb, const char* file_name);
bool is_8_3_format(const char* name);
bool is_8_3_format_directory(const char* name);
void new_directory(int fd_img, bpb_t bpb, const char* dir_name);
void new_file(int fd_img, bpb_t bpb, const char* file_name);
void list_content(int img_fd, bpb_t bpb);
void remove_file(int img_fd, bpb_t bpb, const char* file_name);
void remove_directory(int img_fd, bpb_t bpb, const char* dir_name);
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
            printf("exit command does not take any arguments.\n");
        else if(strcmp(tokens->items[0], "exit") == 0)
        {
            free_tokens(tokens);
            break;
        }
        else if(strcmp(tokens->items[0], "info") == 0 && tokens->size > 1)
            printf("info command does not take any arguments.\n");
        else if (strcmp(tokens->items[0], "info") == 0)
            print_boot_sector_info(bpb);
        else if(strcmp(tokens->items[0], "cd") == 0 && tokens->size > 2)
            printf("cd command does not take more than two arguments.\n");
        else if(strcmp(tokens->items[0], "cd") == 0 && tokens->size < 2)
            printf("cd command requires an argument, none was given.\n");
        else if (strcmp(tokens->items[0], "cd") == 0)
        {
            if(!is_valid_path(img_fd,bpb,tokens->items[1]))
                printf("%s does not exist.\n", tokens->items[1]);
        }
        else if(strcmp(tokens->items[0], "ls") == 0 && tokens->size > 1)
            printf("ls command does not take any arguments.\n");
        else if (strcmp(tokens->items[0], "ls") == 0)
            list_content(img_fd, bpb);
        else if(strcmp(tokens->items[0], "mkdir") == 0 && tokens->size > 2)
            printf("mkdir command does not take more than two arguments.\n");
        else if(strcmp(tokens->items[0], "mkdir") == 0 && tokens->size < 2)
            printf("mkdir command requires an argument, none was given.\n");
        else if (strcmp(tokens->items[0], "mkdir") == 0)
        {
            if(strcmp(tokens->items[1], ".") == 0 || strcmp(tokens->items[1], "..") == 0)
                printf("users can not make . or .. directories.\n");
            else if(is_directory(img_fd, bpb, tokens->items[1]) == false)
            {
                new_directory(img_fd, bpb, tokens->items[1]);
            }
        }
        else if(strcmp(tokens->items[0], "creat") == 0 && tokens->size > 2)
            printf("creat command does not take more than two arguments.\n");
        else if(strcmp(tokens->items[0], "creat") == 0 && tokens->size < 2)
            printf("creat command requires an argument, none was given.\n");
        else if (strcmp(tokens->items[0], "creat") == 0)
        {
            if(is_file(img_fd, bpb, tokens->items[1]) == false)
            {
                new_file(img_fd, bpb, tokens->items[1]);
            }
            else
            {
                printf("file already exists\n");
            }
        }
        else if(strcmp(tokens->items[0], "rm") == 0 && tokens->size > 3)
            printf("rm command does not take more than three arguments\n");
        else if(strcmp(tokens->items[0], "rm") == 0 && tokens->size < 2)
            printf("rm command requires an argument, none was given.\n");
        else if(strcmp(tokens->items[0], "rm") == 0 && tokens->size == 3 && strcmp(tokens->items[1], "-r") != 0)
            printf("unknown flag for rm command, did you mean %s -r %s?\n", tokens->items[0], tokens->items[2]);
        else if (strcmp(tokens->items[0], "rm") == 0)
        {
            if(tokens->size == 3)
            {
                if(is_directory(img_fd, bpb, tokens->items[2]) == true)
                {
                    remove_directory(img_fd, bpb, tokens->items[2]);
                }
                else
                {
                    printf("Directory named %s does not exist in the current directory.\n", tokens->items[1]);
                }
            }
            else
            {
                if(is_file(img_fd, bpb, tokens->items[1]) == true)
                {
                    remove_file(img_fd, bpb, tokens->items[1]);
                }
                else
                {
                    printf("File named %s does not exist in the current directory.\n", tokens->items[1]);
                }
            }
        }
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
    int img_fd = open(img_path, O_RDWR);
    if (img_fd == -1) {
        perror("open failed");
        return EXIT_FAILURE;
    }

    // 2. mount the fat32.img
    bpb_t bpb = mount_fat32(img_fd);

    uint32_t offset = 0;
    uint32_t curr_clus_num = 3;
    uint32_t next_clus_num = 0;
    uint32_t max_clus_num = bpb.BPB_FATSz32 / bpb.BPB_SecPerClus;
    uint32_t min_clus_num = 2;

    // 3. main procees
    main_process(img_fd, img_path, bpb);

    // 4. close all opened files

    // 5. close the fat32.img
    close(img_fd);

    return 0;
}

void remove_directory(int img_fd, bpb_t bpb, const char* dir_name) {
    printf("recursion?\n");
    if (!is_8_3_format(dir_name)) {
        printf("%s is not in FAT32 8.3 format\n", dir_name);
        return;
    }
    //temporarily change current path to use directory_location to find start cluster #
    char original_path[256];
    strncpy(original_path, current_path, sizeof(original_path));

    // Update the current path to include the directory to be removed
    if (strcmp(current_path, "/") != 0) {
        strncat(current_path, "/", sizeof(current_path) - strlen(current_path) - 1);
    }
    strncat(current_path, dir_name, sizeof(current_path) - strlen(current_path) - 1);

    // Call directory_location with the updated current path
    uint32_t dir_cluster = directory_location(img_fd, bpb);

    // Restore the original current path
    strncpy(current_path, original_path, sizeof(current_path));

    if (dir_cluster == 0) {
        printf("Directory %s not found.\n", dir_name);
        return;
    }

    uint32_t sectorSize = bpb.BPB_BytsPerSec;
    uint32_t clusterSize = bpb.BPB_SecPerClus * sectorSize;
    char buffer[clusterSize];
    dentry_t *dirEntry;

    // Iterate over directory entries
    while (dir_cluster != 0xFFFFFFFF) {
        printf("recursion?recursion?\n");
        uint32_t dataRegionOffset = convert_clus_num_to_offset_in_data_region(dir_cluster, bpb);
        ssize_t bytesRead = pread(img_fd, buffer, clusterSize, dataRegionOffset);

        if (bytesRead <= 0) {
            printf("Error reading directory entries.\n");
            return;
        }

        for (uint32_t i = 0; i < bytesRead; i += sizeof(dentry_t)) {
            printf("0\n");
            dirEntry = (dentry_t *)(buffer + i);

            // Check for end of directory
            if (dirEntry->DIR_Name[0] == (uint8_t)0x00) {
                break; // End of directory entries
            }

            // Skip deleted entries and '.' and '..' entries
            if (dirEntry->DIR_Name[0] == (uint8_t)0xE5 || strcmp(dirEntry->DIR_Name, ".") == 0 || strcmp(dirEntry->DIR_Name, "..") == 0) {
                continue;
            }
            // Construct full entry name
            char entryName[12];
            memcpy(entryName, dirEntry->DIR_Name, 11);
            entryName[11] = '\0';
            // Remove file or recursively remove directory
            if (dirEntry->DIR_Attr & 0x10) { // Directory
                printf("here\n");
                remove_directory(img_fd, bpb, entryName);
                printf("Not yet seg fault3\n");
            } else { // File
                printf("here2\n");
                remove_file(img_fd, bpb, entryName);
                printf("Not yet seg fault4\n");
            }
        }
        printf("Not yet seg fault5\n");
        // Get next cluster number from FAT
        uint32_t fatOffset = convert_clus_num_to_offset_in_fat_region(dir_cluster, bpb);
        pread(img_fd, &dir_cluster, sizeof(uint32_t), fatOffset);
        printf("Not yet seg fault6\n");
    }
    printf("Not yet seg fault7\n");
    // Finally, remove the directory itself
    remove_file(img_fd, bpb, dir_name);
}

void remove_file(int img_fd, bpb_t bpb, const char* file_name) {
    if (!is_8_3_format(file_name)) {
        printf("%s is not in FAT32 8.3 format\n", file_name);
        return;
    }

    uint32_t dir_cluster = directory_location(img_fd, bpb);
    if (dir_cluster == 0) {
        printf("Error: Directory not found.\n");
        return;
    }

    uint32_t sectorSize = bpb.BPB_BytsPerSec;
    uint32_t clusterSize = bpb.BPB_SecPerClus * sectorSize;
    char buffer[clusterSize];
    dentry_t *dirEntry;
    bool fileFound = false;
    uint32_t fileFirstCluster;

    while (dir_cluster != 0xFFFFFFFF) {
        printf("here\n");
        uint32_t dataRegionOffset = convert_clus_num_to_offset_in_data_region(dir_cluster, bpb);
        ssize_t bytesRead = pread(img_fd, buffer, clusterSize, dataRegionOffset);

        if (bytesRead <= 0) {
            printf("Error reading directory entries.\n");
            return;
        }
        if (bytesRead == -1) {
            perror("Error reading directory entries");
            return;
        }

        for (uint32_t i = 0; i < bytesRead; i += sizeof(dentry_t)) {
            dirEntry = (dentry_t *)(buffer + i);

            if (dirEntry->DIR_Name[0] == (uint8_t)0x00) break; // End of directory entries

            if (dirEntry->DIR_Name[0] == (uint8_t)0xE5) continue; // Skip deleted entries


            if (strncmp(dirEntry->DIR_Name, file_name, strlen(file_name)) == 0 
            && (dirEntry->DIR_Name[strlen(file_name)] == (uint8_t)0x00 || dirEntry->DIR_Name[strlen(file_name)] == (uint8_t)0x20)) {
                // Check if entry is a file
                if (!(dirEntry->DIR_Attr & 0x10)) {
                    fileFound = true;
                    fileFirstCluster = ((uint32_t)dirEntry->DIR_FstClusHI << 16) | (uint32_t)dirEntry->DIR_FstClusLO;

                    // Mark as deleted
                    dirEntry->DIR_Name[0] = (char)0xE5;
                    if (pwrite(img_fd, dirEntry, sizeof(dentry_t), dataRegionOffset + i) == -1) {
                        perror("Error writing directory entry");
                        return;
                    }
                    break;
                }
            }
        }

        if (fileFound) break;

        // Get next cluster number from FAT
        uint32_t fatOffset = convert_clus_num_to_offset_in_fat_region(dir_cluster, bpb);
        if (pread(img_fd, &dir_cluster, sizeof(uint32_t), fatOffset) == -1) {
            perror("Error reading from FAT");
            return;
        }
    }

    if (!fileFound) {
        printf("File not found.\n");
        return;
    }

    // Free the clusters used by the file
    uint32_t currentCluster = fileFirstCluster;
    while (!is_end_of_file_or_bad_cluster(currentCluster)) {
        uint32_t nextCluster;
        uint32_t fatOffset = convert_clus_num_to_offset_in_fat_region(currentCluster, bpb);
        if (pread(img_fd, &nextCluster, sizeof(uint32_t), fatOffset) == -1) {
            perror("Error reading from FAT");
            return;
        }

        uint32_t freeCluster = 0;
        if (pwrite(img_fd, &freeCluster, sizeof(uint32_t), fatOffset) == -1) {
            perror("Error freeing cluster");
            return;
        }

        currentCluster = nextCluster;
    }
}

void list_content(int img_fd, bpb_t bpb) {
    uint32_t clusterNum;

    // Special handling for root directory
    if (strcmp(current_path, "/") == 0) {
        clusterNum = bpb.BPB_RootClus; // The root cluster number is in the BPB
    } else {
        clusterNum = directory_location(img_fd, bpb); // Get starting cluster of current directory
    }

    if (clusterNum == 0) {
        printf("Error: Current directory not found.\n");
        return;
    }

    uint32_t sectorSize = bpb.BPB_BytsPerSec;
    uint32_t clusterSize = bpb.BPB_SecPerClus * sectorSize;
    uint32_t nextClusterNum;
    char buffer[clusterSize];
    dentry_t *dirEntry;

    bool endOfDirectoryReached = false;

    while (!is_end_of_file_or_bad_cluster(clusterNum) && !endOfDirectoryReached) {
        uint32_t dataRegionOffset = convert_clus_num_to_offset_in_data_region(clusterNum, bpb);
        ssize_t bytesRead = pread(img_fd, buffer, clusterSize, dataRegionOffset);

        if (bytesRead <= 0) {
            printf("Error reading directory entries.\n");
            break;
        }

        for (uint32_t i = 0; i < bytesRead; i += sizeof(dentry_t)) {
            dirEntry = (dentry_t *)(buffer + i);

            // End of directory entries marker
            if (dirEntry->DIR_Name[0] == (uint8_t)0x00) {
                endOfDirectoryReached = true;
                break;
            }
            // Skip deleted entries
            if (dirEntry->DIR_Name[0] == (uint8_t)0xE5) {
                continue; // Correctly skip the deleted file
            }

            // Print directory entry name
            char name[12];
            memcpy(name, dirEntry->DIR_Name, 11);
            name[11] = '\0';
            // Check if it is a directory and apply blue color
            if (dirEntry->DIR_Attr & 0x10) {
                printf("\033[34m%s\033[0m\n", name);  // Blue text for directories
            } else {
                printf("%s\n", name);  // Default text color for files
            }
        }

        // Get next cluster number from FAT
        uint32_t fatOffset = convert_clus_num_to_offset_in_fat_region(clusterNum, bpb);
        pread(img_fd, &nextClusterNum, sizeof(uint32_t), fatOffset);
        clusterNum = nextClusterNum;
    }
}


bool is_valid_path(int fd_img, bpb_t bpb, const char* path) {
    char original_path[256];
    strncpy(original_path, current_path, sizeof(original_path)); // Save the current path

    char temp_path[256];
    if (path[0] == '/') {
        strncpy(temp_path, path, sizeof(temp_path)); // Absolute path
    } else {
        snprintf(temp_path, sizeof(temp_path), "%s/%s", current_path, path); // Relative path
    }

    char *token;
    char *tokens[256];
    int token_count = 0;
    token = strtok(temp_path, "/");
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // Do nothing for current directory reference
        } else if (strcmp(token, "..") == 0) {
            // Navigate one directory up
            if (token_count > 0) {
                token_count--;
                if(token_count == 0)
                {
                    strncpy(current_path, "/", sizeof(current_path));
                    current_path[sizeof(current_path) - 1] = '\0'; // Add null terminator

                    return true;
                }
            }
        } else {
            tokens[token_count++] = token; // Add directory to the path
        }
        token = strtok(NULL, "/");
    }

    char absolute_path[256] = "/";
    for (int i = 0; i < token_count; i++) {
        strcat(absolute_path, tokens[i]);
        if (i < token_count - 1) {
            strcat(absolute_path, "/");
        }
    }

    strncpy(current_path, absolute_path, sizeof(current_path)); // Update current_path

    // Check if the directory exists
    uint32_t cluster_num = directory_location(fd_img, bpb);
    if (cluster_num == 0) {
        strncpy(current_path, original_path, sizeof(current_path)); // Restore original path if not valid
        return false;
    }

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
    //printf("full path copy:%s\n", full_path_copy);
    token = strtok(full_path_copy, "/");
    while (token != NULL) {
        bool found = false;
        do {
            // Read the current directory's entries
            uint32_t dataRegionOffset = bpb.BPB_BytsPerSec * bpb.BPB_RsvdSecCnt + bpb.BPB_NumFATs * bpb.BPB_FATSz32 * bpb.BPB_BytsPerSec;
            dataRegionOffset += (clusterNum - 2) * clusterSize;
            ssize_t bytesRead = pread(fd_img, buffer, clusterSize, dataRegionOffset);

            if (bytesRead <= 0) {
                free(full_path_copy);
                printf("error on read\n");
                return 0;
            }

            for (uint32_t i = 0; i < bytesRead; i += sizeof(dentry_t)) {
                dirEntry = (dentry_t *)(buffer + i);
                if (dirEntry->DIR_Name[0] == (uint8_t)0x00) { // End of directory entries
                    printf("end of directory reached\n");
                    found = false;
                    break;
                }
                if (strncmp(dirEntry->DIR_Name, token, strlen(token)) == 0 && (dirEntry->DIR_Attr & 0x10)) {
                    if (dirEntry->DIR_Name[strlen(token)] == (uint8_t)0x00 || dirEntry->DIR_Name[strlen(token)] == (uint8_t)0x20)
                    {
                        found = true;
                        clusterNum = ((uint32_t)dirEntry->DIR_FstClusHI << 16) | (uint32_t)dirEntry->DIR_FstClusLO;
                        break;
                    }
                }
            }

            if (!found) {
                // Get next cluster number from FAT
                uint32_t fatOffset = convert_clus_num_to_offset_in_fat_region(clusterNum, bpb);
                pread(fd_img, &nextClusterNum, sizeof(uint32_t), fatOffset);
                clusterNum = nextClusterNum;
            }
        } while (!found && !is_end_of_file_or_bad_cluster(clusterNum));

        if (!found) {
            free(full_path_copy);
            printf("nothing found, end of file or bad cluster, ...\n");
            printf("return of is_end_of_file_or_bad_cluster function:%d\n", is_end_of_file_or_bad_cluster(clusterNum));
            return 0;
        }

        token = strtok(NULL, "/");
    }

    free(full_path_copy);
    return clusterNum;
}

bool is_directory(int fd_img, bpb_t bpb, const char* dir_name) {

    if (!is_8_3_format(dir_name) || !is_8_3_format_directory(dir_name)) {
        printf("%s is not in fat32 8.3 format\n", dir_name);
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
        uint32_t dataRegionOffset = bpb.BPB_BytsPerSec * bpb.BPB_RsvdSecCnt + bpb.BPB_NumFATs * bpb.BPB_FATSz32 * bpb.BPB_BytsPerSec;
        dataRegionOffset += (clusterNum - 2) * bufferSize;
        ssize_t bytesRead = pread(fd_img, buffer, bufferSize, dataRegionOffset);

        if (bytesRead <= 0) {
            printf("data region could not be read\n");
            return true;
        }

        for (uint32_t i = 0; i < bytesRead; i += dirEntrySize) {
            dirEntry = (dentry_t *)(buffer + i);

            // End of directory marker
            if (dirEntry->DIR_Name[0] == (uint8_t)0x00) {
                return false;
            }
            // Skip deleted entries and check for directory name match
            if (dirEntry->DIR_Name[0] != (uint8_t)0xE5 && strncmp(dirEntry->DIR_Name, dir_name, strlen(dir_name)) == 0 && 
                (dirEntry->DIR_Attr & 0x10)) {
                    if (dirEntry->DIR_Name[strlen(dir_name)] == (uint8_t)0x00 || dirEntry->DIR_Name[strlen(dir_name)] == (uint8_t)0x20)
                    {
                        printf("directory already exists\n");
                        return true;
                    }
            }
        }
        // Get next cluster number from FAT
        uint32_t fatOffset = convert_clus_num_to_offset_in_fat_region(clusterNum, bpb);
        pread(fd_img, &nextClusterNum, sizeof(uint32_t), fatOffset);
        clusterNum = nextClusterNum;

    } while (!is_end_of_file_or_bad_cluster(clusterNum));
    printf("no directory with the name %s found", dir_name);
    return false;
}

bool is_file(int fd_img, bpb_t bpb, const char* file_name) {

    if (!is_8_3_format(file_name)) {
        printf("%s is not in fat32 8.3 format\n", file_name);
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
        uint32_t dataRegionOffset = bpb.BPB_BytsPerSec * bpb.BPB_RsvdSecCnt + bpb.BPB_NumFATs * bpb.BPB_FATSz32 * bpb.BPB_BytsPerSec;
        dataRegionOffset += (clusterNum - 2) * bufferSize;
        ssize_t bytesRead = pread(fd_img, buffer, bufferSize, dataRegionOffset);

        if (bytesRead <= 0) {
            printf("data region could not be read\n");
            return true;
        }

        for (uint32_t i = 0; i < bytesRead; i += dirEntrySize) {
            dirEntry = (dentry_t *)(buffer + i);

            // End of directory marker
            if (dirEntry->DIR_Name[0] == (uint8_t)0x00) {
                return false;
            }
            //printf("dirEntry->DIR_Name[0] is 0x%X\n", dirEntry->DIR_Name[0]);
            //printf("dirEntry->DIR_Name is %s\n", dirEntry->DIR_Name);
            //printf("dirEntry->DIR_Attr is 0x%X\n", dirEntry->DIR_Attr);
            //printf("dirEntry->DIR_Name[strlen(file_name)] is 0x%X\n", dirEntry->DIR_Name[strlen(file_name)]);
            // Skip deleted entries and check for file name match
            if (dirEntry->DIR_Name[0] != (uint8_t)0xE5 &&
                strncmp(dirEntry->DIR_Name, file_name, strlen(file_name)) == 0 && 
                ((dirEntry->DIR_Attr == 0x20) || (dirEntry->DIR_Attr == 0x0F))) {
                    if (dirEntry->DIR_Name[strlen(file_name)] == (uint8_t)0x00 || dirEntry->DIR_Name[strlen(file_name)] == (uint8_t)0x20)
                    {
                        return true;
                    }
            }
        }

        // Get next cluster number from FAT
        uint32_t fatOffset = convert_clus_num_to_offset_in_fat_region(clusterNum, bpb);
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
            if ((dot_encountered && name_len > 3) || (!dot_encountered && name_len > 8))
                return false; // Extension or name part too long
        }
    }
    return true;
}

bool is_8_3_format_directory(const char* name) {
    for (int i = 0; name[i] != '\0'; i++) {
        char upper_char = toupper((unsigned char)name[i]);

        // Check if the character is not the same as its uppercase version
        if (name[i] != upper_char)
            return false;
    }
    return true;
}

void new_directory(int fd_img, bpb_t bpb, const char* dir_name) {
    // Check if the directory name is in FAT32 8.3 format
    if (!is_8_3_format(dir_name) || !is_8_3_format_directory(dir_name)) {
        printf("Directory name is not in FAT32 8.3 format\n");
        return;
    }

    // Allocate a cluster for the new directory
    uint32_t free_cluster = alloca_cluster(fd_img, bpb);
    if (free_cluster == 0) {
        printf("Failed to allocate a cluster for the new directory\n");
        return;
    }

    clear_cluster(fd_img, free_cluster, bpb);

    // Create a directory entry for the new directory
    dentry_t new_dir_entry = {0};
    strncpy(new_dir_entry.DIR_Name, dir_name, 11);
    new_dir_entry.DIR_Attr = 0x10; // Directory attribute
    new_dir_entry.DIR_FstClusHI = (free_cluster >> 16) & 0xFFFF;
    new_dir_entry.DIR_FstClusLO = free_cluster & 0xFFFF;
    new_dir_entry.DIR_FileSize = 0; // A directory's size is 0

    // Find the location of the parent directory and append the new directory entry
    uint32_t parent_dir_cluster;
    if (strcmp(current_path, "/") == 0) {
        parent_dir_cluster = bpb.BPB_RootClus; // Use root cluster if in root directory
    } else {
        parent_dir_cluster = directory_location(fd_img, bpb); // Else get the parent's cluster
    }
    append_dir_entry(fd_img, &new_dir_entry, parent_dir_cluster, bpb);

    dentry_t dot_entry = {0};
    strncpy(dot_entry.DIR_Name, ".          ", 11); // 11 characters, padded with spaces
    dot_entry.DIR_Attr = 0x10; // Directory attribute
    dot_entry.DIR_FstClusHI = (free_cluster >> 16) & 0xFFFF;
    dot_entry.DIR_FstClusLO = free_cluster & 0xFFFF;

    dentry_t dotdot_entry = {0};
    strncpy(dotdot_entry.DIR_Name, "..         ", 11); // 11 characters, padded with spaces
    dotdot_entry.DIR_Attr = 0x10; // Directory attribute
    uint32_t parent_cluster = (strcmp(current_path, "/") == 0) ? bpb.BPB_RootClus : directory_location(fd_img, bpb);
    dotdot_entry.DIR_FstClusHI = (parent_cluster >> 16) & 0xFFFF;
    dotdot_entry.DIR_FstClusLO = parent_cluster & 0xFFFF;

    // Write '.' and '..' entries to the new directory
    write_dir_entry(fd_img, &dot_entry, convert_clus_num_to_offset_in_data_region(free_cluster, bpb));
    write_dir_entry(fd_img, &dotdot_entry, convert_clus_num_to_offset_in_data_region(free_cluster, bpb) + sizeof(dentry_t));
    
    //add an end-of-directory marker
    dentry_t end_of_dir_entry = {0};
    end_of_dir_entry.DIR_Name[0] = 0x00;  // End-of-directory marker

    // The offset for the end-of-directory entry should be right after the '.' and '..' entries
    uint32_t end_of_dir_offset = convert_clus_num_to_offset_in_data_region(free_cluster, bpb)
                                 + 2 * sizeof(dentry_t); // Size of two entries ('.' and '..')

    // Write the end-of-directory entry to the directory
    write_dir_entry(fd_img, &end_of_dir_entry, end_of_dir_offset);

}

void new_file(int fd_img, bpb_t bpb, const char* file_name) {
    // Convert file_name to FAT32 8.3 format
    // Assuming a function that does this conversion

    // Allocate a cluster for the new file
    uint32_t free_cluster = alloca_cluster(fd_img, bpb);
    if (free_cluster == 0) {
        printf("Failed to allocate a cluster for the new file\n");
        return;
    }

    clear_cluster(fd_img, free_cluster, bpb);

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
        perror("Error writing directory entry");
    }
}

void append_dir_entry(int fd, dentry_t *new_dentry, uint32_t clus_num, bpb_t bpb) {
    uint32_t curr_clus_num = clus_num;
    uint32_t sectorSize = bpb.BPB_BytsPerSec;
    uint32_t clusterSize = bpb.BPB_SecPerClus * sectorSize;
    uint32_t bytesProcessed = 0;
    bool spaceFound = false;

    while (true) {
        uint32_t data_offset = convert_clus_num_to_offset_in_data_region(curr_clus_num, bpb);

        for (bytesProcessed = 0; bytesProcessed < clusterSize; bytesProcessed += sizeof(dentry_t)) {
            dentry_t dentry;
            ssize_t rd_bytes = pread(fd, &dentry, sizeof(dentry_t), data_offset + bytesProcessed);
            if (rd_bytes != sizeof(dentry_t)) {
                printf("Failed to read directory entry\n");
                return;
            }

            if (dentry.DIR_Name[0] == 0x00 || dentry.DIR_Name[0] == (uint8_t)0xE5) {
                ssize_t wr_bytes = pwrite(fd, new_dentry, sizeof(dentry_t), data_offset + bytesProcessed);
                if (wr_bytes != sizeof(dentry_t)) {
                    printf("Failed to write directory entry\n");
                    return;
                }
                return; // Entry written, exit function
            }
        }

        // Check if next cluster is available in the chain
        uint32_t fat_offset = convert_clus_num_to_offset_in_fat_region(curr_clus_num, bpb);
        uint32_t next_clus_num;
        pread(fd, &next_clus_num, sizeof(uint32_t), fat_offset);

        if (next_clus_num == 0xFFFFFFFF) {
            // Allocate a new cluster and link it
            uint32_t new_clus_num = alloca_cluster(fd, bpb);
            if (new_clus_num == 0) {
                printf("No free cluster available for directory expansion\n");
                return;
            }
            pwrite(fd, &new_clus_num, sizeof(uint32_t), fat_offset); // Link the new cluster
            curr_clus_num = new_clus_num;
        } else {
            curr_clus_num = next_clus_num; // Move to next cluster in the chain
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
    printf("Position of Root Cluster: %u\n", bpb.BPB_RootClus);
    printf("Bytes Per Sector: %u\n", bpb.BPB_BytsPerSec);
    printf("Sectors Per Cluster: %u\n", bpb.BPB_SecPerClus);
    // Calculate and print total number of clusters in data region
    uint32_t totalClusters = (bpb.BPB_TotSec32 - bpb.BPB_RsvdSecCnt) / bpb.BPB_SecPerClus;
    printf("Total # of Clusters in Data Region: %u\n", totalClusters);
    printf("# of Entries in One FAT: %u\n", bpb.BPB_NumFATs);
    uint32_t imageSize = bpb.BPB_TotSec32 * bpb.BPB_BytsPerSec;
    printf("Size of Image (in bytes): %u\n", imageSize);
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

    if (rd_bytes != sizeof(bpb_t)) {
        printf("request %zu bytes, but read %zu bytes\n", sizeof(bpb_t), rd_bytes);
        close(img_fd);
        exit(EXIT_FAILURE);
    }
    printf("               INITIAL VALUES:\n");
    printf("===============================================\n");
    printf("BS_jmpBoot is %.3s\n", bpb.BS_jmpBoot);
    printf("BS_OEMName is %.8s\n", bpb.BS_OEMName);
    printf("BPB_BytsPerSec is %u\n", bpb.BPB_BytsPerSec);
    printf("BPB_SecPerClus is %u\n", bpb.BPB_SecPerClus);
    printf("BPB_RsvdSecCnt is %u\n", bpb.BPB_RsvdSecCnt);
    printf("BPB_NumFATs is %u\n", bpb.BPB_NumFATs);
    printf("BPB_RootEntCnt is %u\n", bpb.BPB_RootEntCnt);
    printf("BPB_TotSec16 is %u\n", bpb.BPB_TotSec16);
    printf("BPB_Media is %u\n", bpb.BPB_Media);
    printf("BPB_FATSz32 is %u\n", bpb.BPB_FATSz32);
    printf("BPB_SecPerTrk is %u\n", bpb.BPB_SecPerTrk);
    printf("BPB_NumHeads is %u\n", bpb.BPB_NumHeads);
    printf("BPB_HiddSec is %u\n", bpb.BPB_HiddSec);
    printf("BPB_TotSec32 is %u\n", bpb.BPB_TotSec32);
    printf("BPB_RootClus is %u\n", bpb.BPB_RootClus);
    printf("===============================================\n");

    return bpb;
}

uint32_t convert_offset_to_clus_num_in_fat_region(uint32_t offset, bpb_t bpb) {
    uint32_t fat_region_offset = bpb.BPB_BytsPerSec * bpb.BPB_RsvdSecCnt;
    return (offset - fat_region_offset)/4;
}

uint32_t convert_clus_num_to_offset_in_fat_region(uint32_t clus_num, bpb_t bpb) {
    uint32_t fat_region_offset = bpb.BPB_BytsPerSec * bpb.BPB_RsvdSecCnt;
    return fat_region_offset + clus_num * 4;
}

uint32_t convert_clus_num_to_offset_in_data_region(uint32_t clus_num, bpb_t bpb) {
    uint32_t data_region_offset = bpb.BPB_BytsPerSec * bpb.BPB_RsvdSecCnt + bpb.BPB_NumFATs * bpb.BPB_FATSz32 * bpb.BPB_BytsPerSec;
    return data_region_offset + (clus_num - 2) * bpb.BPB_BytsPerSec;
}

void clear_cluster(int fd, uint32_t cluster_num, bpb_t bpb) {
    uint32_t clusterSize = bpb.BPB_SecPerClus * bpb.BPB_BytsPerSec;
    char *emptyData = (char *)calloc(clusterSize, sizeof(char)); // Allocate and zero initialize

    if (emptyData == NULL) {
        perror("Failed to allocate memory for clearing cluster");
        return;
    }

    uint32_t offset = convert_clus_num_to_offset_in_data_region(cluster_num, bpb);
    ssize_t bytesWritten = pwrite(fd, emptyData, clusterSize, offset);

    if (bytesWritten != clusterSize) {
        perror("Failed to clear cluster");
    }

    free(emptyData);
}

uint32_t alloca_cluster(int fd, bpb_t bpb) {
    uint32_t min_clus_num = 2;
    uint32_t max_clus_num = bpb.BPB_FATSz32;
    uint32_t clus_clus_num = min_clus_num;
    uint32_t next_clus_num;
    
    while (clus_clus_num <= max_clus_num) {
        uint32_t offset = convert_clus_num_to_offset_in_fat_region(clus_clus_num, bpb);
        ssize_t bytes_read = pread(fd, &next_clus_num, sizeof(uint32_t), offset);

        // Check if pread was successful
        if (bytes_read != sizeof(uint32_t)) {
            perror("Error reading FAT entry");
            return 0; // Indicate failure
        }

        // Check if the cluster is free
        if (next_clus_num == 0) {
            uint32_t end_of_chain = 0x0FFFFFFF;
            ssize_t bytes_written = pwrite(fd, &end_of_chain, sizeof(uint32_t), offset);

            // Check if pwrite was successful
            if (bytes_written != sizeof(uint32_t)) {
                perror("Error writing FAT entry");
                return 0; // Indicate failure
            }

            return clus_clus_num; // Return the allocated cluster number
        } else {
            ++clus_clus_num;
        }
    }

    printf("No free cluster found\n");
    return 0; // No free cluster found
}


void extend_cluster_chain(int fd, uint32_t *current_clus_num_ptr, dentry_t *dentry_ptr, bpb_t bpb) {
    // Allocate a new cluster
    uint32_t new_clus_num = alloca_cluster(fd, bpb);
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
        uint32_t offset = convert_clus_num_to_offset_in_data_region(*current_clus_num_ptr, bpb);
        write_dir_entry(fd, dentry_ptr, offset);
    } else {
        // The file or directory already has clusters. Extend the chain.
        uint32_t final_clus_num = *current_clus_num_ptr;
        uint32_t final_offset = convert_clus_num_to_offset_in_fat_region(final_clus_num, bpb);
        pwrite(fd, &new_clus_num, sizeof(uint32_t), final_offset);

        // Mark the new cluster as the end of the chain
        uint32_t new_offset = convert_clus_num_to_offset_in_fat_region(new_clus_num, bpb);
        uint32_t end_of_file = 0xFFFFFFFF;
        pwrite(fd, &end_of_file, sizeof(uint32_t), new_offset);
    }
}

bool is_end_of_file_or_bad_cluster(uint32_t clus_num) {
    // Define the values that indicate the end of a file and bad clusters in FAT32.
    uint32_t fat32_end_of_file = 0x0FFFFFFF;
    uint32_t fat32_end_of_file2 = 0xFFFFFFFF;
    uint32_t fat32_bad_cluster_min = 0x0FFFFFF8;
    uint32_t fat32_bad_cluster_max = 0x0FFFFFFF;

    // Check if the cluster number falls within the range of bad clusters or is the end of the file.
    if ((clus_num >= fat32_bad_cluster_min && clus_num <= fat32_bad_cluster_max) || clus_num == fat32_end_of_file || clus_num == fat32_end_of_file2) {
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
