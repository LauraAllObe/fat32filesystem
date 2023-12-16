# Filesystem
Design and implement a simple, user-space, shell-like utility that is capable of
interpreting a FAT32 file system image. The program must understand the basic commands to
manipulate the given file system image, must not corrupt the file system image, 
and should be robust. 
###### *Notes: some implementations are partial, such as the append command.*

# Demonstration Video:
[![](https://drive.google.com/uc?export=view&id=1W8ULly1Y_xJhkidqaRik8IN0dI4PwWW9)](https://youtu.be/PwfMXMGMbD8)

## Group Members
- **Jeyma Rodrìguez**: jdr21@fsu.edu
- **Autumn Harp**: aom21a@fsu.edu
- **Laura Obermaier**: lao21@fsu.edu
## Division of Labor

### Part 1: : Mount the Image File [5 points]
- **Responsibilities**: Greet the user with a prompt that indicates the current working directory.
Open, mount, and close the fat32.img. Add basic tokenizaiton/parsing (from project 1) and functions.
Implement exit and info commands. A basic Makefile (also from project 1). Integrate given functions
and initial setup (such as main_process).
- **Assigned to**: Laura Obermaier

### Part 2: Navigation [10 points]
- **Responsibilities**: Handles the cd and ls commands and subsequent errors appropriately. 
Maintains current working directory state using a global variable. Functions for
checking if a directory entry is a file or directory and locating the starting cluster number.
- **Assigned to**: Laura Obermaier

### Part 3: Create [15 points]
- **Responsibilities**: Handles the creat and mkdir commands and subsequent errors appropriately.
On creat and mkdir, directory entries marked as deleted are cleared.
- **Assigned to**: Laura Obermaier

### Part 4: Read [20 points]
- **Responsibilities**: Handles file operations such as opening, closing, listing, seeking, and 
reading within the current working directory. It includes commands to open files with specific modes
(read-only, write-only, or both), maintain a structured record of opened files, and manage file 
access and offsets. The system also provides functionalities to close files, list all opened files 
with their details, and set or adjust file reading/writing offsets. 
- **Assigned to**: Jeyma Rodriguez

### Part 5: Update [10 points]
- **Responsibilities**: Allow users to append to a file. Must be non-space seperated and an open
file with write permission. Note this implementation does not append to the end of a long file such
as LONGFILE. Might not work in subdirectories. Only takes one token not "".
- **Assigned to**: Autumn Harp

### Part 6: Delete [10 points]
- **Responsibilities**: Handles the rm command and subsequent errors appropriately. These are 
marked as deleted (later cleared by mkdir and creat). Only closed files can be deleted.
- **Assigned to**: Laura Obermaier

### Extra Credit (rm -r)
- **Responsibilities**: Handles the rm -r command and subsequent errors appropriately. 
- **Assigned to**: Laura Obermaier

## File Listing
```
shell/
│
├── src/
│ └── filesys.c
├── fat32.img
├── README.md
└── Makefile
```
## How to Compile & Execute

### Requirements
- **Compiler**: `gcc`

### Compilation
make

### Execution
./bin/filesys fat32.img

## Bugs
Append does not work on long files (i.e. LONGFILE), can only append one token not "", and might not
work in subidrectories.

## Extra Credit
- **Extra Credit rm -r command: Laura Obermaier**: 

## Considerations
N/A
