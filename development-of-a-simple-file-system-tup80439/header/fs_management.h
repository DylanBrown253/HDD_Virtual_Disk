#ifndef FS_MANAGEMENT_H
#define FS_MANAGEMENT_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h> // For off_t

// Constants
#define MAX_DISK_NAME_LENGTH 256
#define MAX_FILE_SIZE (4096 * BLOCK_SIZE) // 16 MB
#define DATA_BLOCKS_START 4096            // As per your boot sector
#define MAX_FILE_DESCRIPTORS 32
#define BLOCK_ARRAY_SIZE 4096

// File Descriptor Structure
typedef struct {
    int is_open;          // 1 if open, 0 if closed
    int file_index;       // Index into your rootDir array
    size_t offset;        // Current file offset (seek pointer)
} file_descriptor;

// Boot Sector Structure
typedef struct {
    int dataOffset;
    int locationOfBoot;
    int sizeOfBoot;
    int fat1_location;
    int sizeOfFat1;
    int fat2_location;
    int sizeOfFat2;
    int root_location;
    int num_files; // Number of files in root
} boot_sector;

// File Entry Structure
typedef struct {
    int isFile;            // 1 if this entry represents a file, 0 otherwise
    int numOpen;           // Number of times the file is open
    int fPointer;          // File pointer (unused in this struct, could be removed)
    char filename[16];     // File name (15 chars max + null terminator)
    int firstDataBlock;    // Index of the first data block in the FAT
    size_t sizeInBytes;    // Size of the file in bytes
    char timeCreated[9];   // Time of creation (hh:mm:ss)
    char dateCreated[9];   // Date of creation (mm/dd/yy)
} files;

// Global Variables
extern char BLOCK_ARRAY[BLOCK_ARRAY_SIZE];
extern int is_mounted; // 0 = not mounted, 1 = mounted

extern file_descriptor file_descriptors[MAX_FILE_DESCRIPTORS];
extern boot_sector bs;
extern int FAT1[4096];
extern int FAT2[4096];
extern files rootDir[64];
extern char mounted_disk_name[MAX_DISK_NAME_LENGTH];

// Function Prototypes

// Initialization Functions
void initFAT(int FAT[]);
int make_fs(char *disk_name);
int mount_fs(char *disk_name);
int unmount_fs(char *disk_name);
int write_to_block(int block_num, void *data, size_t data_size);

// File System Functions
int fs_open(char *fname);
int fs_close(int fildes);
int fs_create(char *fname);
int fs_delete(char *fname);
int fs_read(int fildes, void *buf, size_t nbyte);
int fs_write(int fildes, void *buf, size_t nbyte);
int fs_get_filesize(int fildes);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);

#endif // FS_MANAGEMENT_H
