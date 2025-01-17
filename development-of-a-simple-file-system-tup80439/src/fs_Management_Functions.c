#include "fs_management.h"
#include <stdio.h>
#include "disk.h"
#include <string.h>
#include <time.h>
#include <sys/types.h> // For off_t

// Global variable definitions
char BLOCK_ARRAY[BLOCK_ARRAY_SIZE];
int is_mounted = 0; // 0 = not mounted, 1 = mounted

file_descriptor file_descriptors[MAX_FILE_DESCRIPTORS];

boot_sector bs;

int FAT1[4096];
int FAT2[4096];
files rootDir[64];

char mounted_disk_name[MAX_DISK_NAME_LENGTH];

int write_to_block(int block_num, void *data, size_t data_size) {
    if (data_size > BLOCK_SIZE) {
        fprintf(stderr, "Error: Data size (%zu bytes) exceeds block size (%d bytes)\n", data_size, BLOCK_SIZE);
        return -1;
    }

    char buffer[BLOCK_SIZE] = {0}; // Zero-initialize the buffer
    memcpy(buffer, data, data_size); // Copy data into the buffer

    if (block_write(block_num, buffer) == -1) {
        fprintf(stderr, "Error: Failed to write to block %d\n", block_num);
        return -1;
    }

    return 0; // Success
}


void initFAT(int FAT[]){
  for (int i = 0; i < 4096; i++)
  {
    FAT[i] = -2;
  }
}

//make the file system by calling make_disk
int make_fs(char *disk_name) {
    initFAT(FAT1);
    initFAT(FAT2);
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        file_descriptors[i].is_open = 0;
    }

    if (make_disk(disk_name) == -1) {
        printf("Disk could not be created\n");
        return -1;
    }

    if (open_disk(disk_name) == -1) {
        printf("Disk could not be opened\n");
        return -1;
    }

    // Initialize the boot sector
    bs.dataOffset = 4096; 
    bs.locationOfBoot = 0;
    bs.sizeOfBoot = 1;    
    bs.fat1_location = 100; // Block index for FAT1
    bs.sizeOfFat1 = 4; 
    bs.fat2_location = 200; // Block index for FAT2
    bs.sizeOfFat2 = 4;
    bs.root_location = 300; // Block index for root directory
    bs.num_files = 0;  

    for (int i = 0; i < 64; i++) {
        rootDir[i].isFile = 0;
    }

    // Write boot sector
    if (write_to_block(0, &bs, sizeof(bs)) == -1) {
        close_disk();
        return -1;
    }

    // Write FAT1 across 4 blocks
    for (int i = 0; i < bs.sizeOfFat1; i++) {
        if (write_to_block(bs.fat1_location + i, &FAT1[i * (BLOCK_SIZE / sizeof(int))], BLOCK_SIZE) == -1) {
            close_disk();
            return -1;
        }
    }

    // Write FAT2 across 4 blocks
    for (int i = 0; i < bs.sizeOfFat2; i++) {
        if (write_to_block(bs.fat2_location + i, &FAT2[i * (BLOCK_SIZE / sizeof(int))], BLOCK_SIZE) == -1) {
            close_disk();
            return -1;
        }
    }

    // Write the root directory
    if (write_to_block(bs.root_location, rootDir, sizeof(rootDir)) == -1) {
        close_disk();
        return -1;
    }

    // Ensure the disk is closed to reset the active state
    close_disk();

    return 0;
}

//mounts a file system that is stored on the virtual disk with name disk_name
//Using the mount operation the disk becomes ready to use
int mount_fs(char *disk_name) {
    if (is_mounted) {
        fprintf(stderr, "Error: File system is already mounted.\n");
        return -1;
    }

    // Attempt to open the disk only if it is not already open
    if (open_disk(disk_name) == -1) {
        fprintf(stderr, "Error: Could not open disk '%s'.\n", disk_name);
        return -1;
    }

    strncpy(mounted_disk_name, disk_name, MAX_DISK_NAME_LENGTH - 1);
    mounted_disk_name[MAX_DISK_NAME_LENGTH - 1] = '\0'; // Ensure null-termination


    // Read the boot sector (block 0)
    if (block_read(0, (char *)&bs) == -1) {
        fprintf(stderr, "Error: Failed to read boot sector.\n");
        close_disk(); // Close the disk if reading fails
        return -1;
    }

    // Verify the boot sector
    if (bs.sizeOfBoot <= 0 || bs.fat1_location <= 0 || bs.root_location <= 0) {
        fprintf(stderr, "Error: Invalid boot sector.\n");
        close_disk(); // Close the disk if verification fails
        return -1;
    }

    int entries_per_block = BLOCK_SIZE / sizeof(int);

    // Read FAT1
    for (int i = 0; i < bs.sizeOfFat1; i++) {
        if (block_read(bs.fat1_location + i, (char *)&FAT1[i * entries_per_block]) == -1) {
            fprintf(stderr, "Error: Failed to read FAT1.\n");
            close_disk();
            return -1;
        }
    }

    // Read FAT2
    for (int i = 0; i < bs.sizeOfFat2; i++) {
        if (block_read(bs.fat2_location + i, (char *)&FAT2[i * entries_per_block]) == -1) {
            fprintf(stderr, "Error: Failed to read FAT2.\n");
            close_disk();
            return -1;
        }
    }

    // Read the root directory
    if (block_read(bs.root_location, (char *)rootDir) == -1) {
        fprintf(stderr, "Error: Failed to read root directory.\n");
        close_disk();
        return -1;
    }

    is_mounted = 1; // Mark the file system as mounted
    printf("File system successfully mounted.\n");
    return 0;
}

int unmount_fs(char *disk_name) {
    if (!is_mounted) {
        fprintf(stderr, "Error: File system is not mounted.\n");
        return -1;
    }

    // Compare the provided disk name with the mounted disk name
    if (strcmp(disk_name, mounted_disk_name) != 0) {
        fprintf(stderr, "Error: Disk name '%s' does not match the mounted disk '%s'.\n", disk_name, mounted_disk_name);
        return -1;
    }

    // Proceed with unmounting
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (file_descriptors[i].is_open) {
            fs_close(i);
        }
    }

    // No need to open the disk again since it's already open

    int entries_per_block = BLOCK_SIZE / sizeof(int);

    // Write FAT1 to disk
    for (int i = 0; i < bs.sizeOfFat1; i++) {
        if (write_to_block(bs.fat1_location + i, &FAT1[i * entries_per_block], BLOCK_SIZE) == -1) {
            fprintf(stderr, "Error: Failed to write FAT1 to disk.\n");
            goto cleanup;
        }
    }

    // Write FAT2 to disk
    for (int i = 0; i < bs.sizeOfFat2; i++) {
        if (write_to_block(bs.fat2_location + i, &FAT2[i * entries_per_block], BLOCK_SIZE) == -1) {
            fprintf(stderr, "Error: Failed to write FAT2 to disk.\n");
            goto cleanup;
        }
    }

    // Write root directory to disk
    if (write_to_block(bs.root_location, rootDir, sizeof(rootDir)) == -1) {
        fprintf(stderr, "Error: Failed to write root directory to disk.\n");
        goto cleanup;
    }

    // Mark as unmounted and close the disk
    is_mounted = 0;
    if (close_disk() == -1) {
        fprintf(stderr, "Error: Failed to close the disk.\n");
        return -1;
    }

    printf("File system successfully unmounted.\n");
    return 0;

cleanup:
    close_disk();  // Ensure the disk is closed in case of an error
    is_mounted = 0;
    return -1;
}

//fs functions
int fs_open(char *fname) {
    // Find the file in rootDir
    int file_index = -1;
    for (int i = 0; i < 64; i++) {
        if (rootDir[i].isFile && strcmp(rootDir[i].filename, fname) == 0) {
            file_index = i;
            break;
        }
    }
    if (file_index == -1) {
        fprintf(stderr, "Error: File not found.\n");
        return -1;
    }

    // Find an available file descriptor
    for (int fd = 0; fd < MAX_FILE_DESCRIPTORS; fd++) {
        if (!file_descriptors[fd].is_open) {
            file_descriptors[fd].is_open = 1;
            file_descriptors[fd].file_index = file_index;
            file_descriptors[fd].offset = 0;
            return fd;
        }
    }

    fprintf(stderr, "Error: Maximum number of file descriptors reached.\n");
    return -1;
}

int fs_close(int fildes) {
    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS) {
        fprintf(stderr, "Error: Invalid file descriptor.\n");
        return -1;
    }

    if (!file_descriptors[fildes].is_open) {
        fprintf(stderr, "Error: File descriptor not open.\n");
        return -1;
    }

    file_descriptors[fildes].is_open = 0;
    return 0;
}

int fs_create(char *fname) {
    if (!is_mounted) {
        fprintf(stderr, "Error: File system is not mounted.\n");
        return -1;
    }

    if (fname == NULL || strlen(fname) == 0) {
        fprintf(stderr, "Error: File name cannot be null or empty.\n");
        return -1;
    }

    if (strlen(fname) > 15) {
        fprintf(stderr, "Error: File name too long.\n");
        return -1;
    }

    // Check if the file already exists
    for (int i = 0; i < 64; i++) {
        if (rootDir[i].isFile && strcmp(rootDir[i].filename, fname) == 0) {
            fprintf(stderr, "Error: File already exists.\n");
            return -1;
        }
    }

    // Find an empty slot in the root directory
    for (int i = 0; i < 64; i++) {
        if (!rootDir[i].isFile) {
            // Fill out each member of the files struct
            rootDir[i].isFile = 1;           // Mark as a valid file
            rootDir[i].numOpen = 0;          // File is not open yet
            rootDir[i].fPointer = 0;         // Not used, but initialized to 0
            strncpy(rootDir[i].filename, fname, 15); // Copy file name
            rootDir[i].filename[15] = '\0';  // Ensure null-termination
            rootDir[i].firstDataBlock = -1;  // No data blocks allocated yet
            rootDir[i].sizeInBytes = 0;      // Initial file size is 0

            // Set timeCreated and dateCreated
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            snprintf(rootDir[i].timeCreated, sizeof(rootDir[i].timeCreated), "%02d:%02d:%02d",
                     t->tm_hour, t->tm_min, t->tm_sec);
            snprintf(rootDir[i].dateCreated, sizeof(rootDir[i].dateCreated), "%02d/%02d/%02d",
                     t->tm_mon + 1, t->tm_mday, (t->tm_year + 1900) % 100); // Last two digits of the year

            bs.num_files++; // Increment the file count in the boot sector

            printf("File '%s' created and added to rootDir at index %d.\n", fname, i);
            return 0;
        }
    }

    fprintf(stderr, "Error: Maximum number of files reached.\n");
    return -1;
}

int fs_delete(char *fname) {
    if (!is_mounted) {
        fprintf(stderr, "Error: File system is not mounted.\n");
        return -1;
    }

    if (fname == NULL || strlen(fname) == 0) {
        fprintf(stderr, "Error: File name cannot be null or empty.\n");
        return -1;
    }

    // Find the file in rootDir
    int file_index = -1;
    for (int i = 0; i < 64; i++) {
        if (rootDir[i].isFile && strcmp(rootDir[i].filename, fname) == 0) {
            file_index = i;
            break;
        }
    }
    if (file_index == -1) {
        fprintf(stderr, "Error: File '%s' not found.\n", fname);
        return -1;
    }

    // Check if the file is open in any file descriptor
    for (int fd = 0; fd < MAX_FILE_DESCRIPTORS; fd++) {
        if (file_descriptors[fd].is_open && file_descriptors[fd].file_index == file_index) {
            fprintf(stderr, "Error: File '%s' is currently open.\n", fname);
            return -1;
        }
    }

    // Now, proceed to delete the file
    // First, free all data blocks used by the file
    int current_block = rootDir[file_index].firstDataBlock;
    while (current_block != -1) {
        if (current_block < 0 || current_block >= 4096) {
            fprintf(stderr, "Error: Invalid block number %d in FAT chain.\n", current_block);
            break;
        }
        int next_block = FAT1[current_block];
        FAT1[current_block] = -2; // Mark as free
        FAT2[current_block] = -2; // Mark as free in the copy
        current_block = next_block;
    }

    // Remove the file's entry from rootDir
    rootDir[file_index].isFile = 0; // Mark slot as free
    // Optionally clear other fields
    memset(&rootDir[file_index], 0, sizeof(files)); // Clear the entire structure

    // Decrease the number of files
    if (bs.num_files > 0) {
        bs.num_files--;
    }

    printf("File '%s' deleted successfully.\n", fname);
    return 0;
}

int fs_read(int fildes, void *buf, size_t nbyte) {
    if (!is_mounted) {
        fprintf(stderr, "Error: File system is not mounted.\n");
        return -1;
    }

    // Validate the file descriptor
    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS || !file_descriptors[fildes].is_open) {
        fprintf(stderr, "Error: Invalid or closed file descriptor.\n");
        return -1;
    }

    int file_index = file_descriptors[fildes].file_index;
    size_t file_offset = file_descriptors[fildes].offset;
    size_t file_size = rootDir[file_index].sizeInBytes;

    // Check if the file pointer is at or beyond the end of the file
    if (file_offset >= file_size) {
        return 0; // Nothing to read
    }

    // Calculate the number of bytes to read
    size_t bytes_to_read = nbyte;
    if (file_offset + nbyte > file_size) {
        bytes_to_read = file_size - file_offset;
    }

    size_t bytes_remaining = bytes_to_read;
    size_t buffer_offset = 0; // Offset into buf
    size_t block_offset = file_offset % BLOCK_SIZE;
    int block_index_within_file = file_offset / BLOCK_SIZE;

    // Get the starting data block
    int current_block = rootDir[file_index].firstDataBlock;
    if (current_block == -1) {
        // No data blocks allocated yet
        return 0;
    }

    // Traverse to the starting block
    for (int i = 0; i < block_index_within_file; i++) {
        current_block = FAT1[current_block];
        if (current_block == -1) {
            // Reached end of file before expected
            return -1;
        }
    }

    while (bytes_remaining > 0 && current_block != -1) {
        // Read the data block
        char block_data[BLOCK_SIZE];
        if (block_read(bs.dataOffset + current_block, block_data) == -1) {
            fprintf(stderr, "Error: Failed to read data block %d.\n", current_block);
            return -1;
        }

        size_t bytes_in_block = BLOCK_SIZE - block_offset;
        size_t bytes_to_copy = bytes_remaining < bytes_in_block ? bytes_remaining : bytes_in_block;

        memcpy((char *)buf + buffer_offset, block_data + block_offset, bytes_to_copy);

        buffer_offset += bytes_to_copy;
        bytes_remaining -= bytes_to_copy;
        file_offset += bytes_to_copy;
        block_offset = 0; // Reset block offset for subsequent blocks

        // Move to next block
        current_block = FAT1[current_block];
    }

    // Update the file descriptor's offset
    file_descriptors[fildes].offset = file_offset;

    // Return the number of bytes actually read
    return bytes_to_read - bytes_remaining;
}

int fs_write(int fildes, void *buf, size_t nbyte) {
    if (!is_mounted) {
        fprintf(stderr, "Error: File system is not mounted.\n");
        return -1;
    }

    // Validate the file descriptor
    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS || !file_descriptors[fildes].is_open) {
        fprintf(stderr, "Error: Invalid or closed file descriptor.\n");
        return -1;
    }

    int file_index = file_descriptors[fildes].file_index;
    size_t file_offset = file_descriptors[fildes].offset;
    //size_t file_size = rootDir[file_index].sizeInBytes;

    // Check for maximum file size
    if (file_offset + nbyte > MAX_FILE_SIZE) {
        nbyte = MAX_FILE_SIZE - file_offset;
        if (nbyte == 0) {
            fprintf(stderr, "Error: Maximum file size reached.\n");
            return 0;
        }
    }

    size_t bytes_to_write = nbyte;
    size_t bytes_written = 0;
    size_t buffer_offset = 0; // Offset into buf
    size_t block_offset = file_offset % BLOCK_SIZE;
    int block_index_within_file = file_offset / BLOCK_SIZE;

    // Get the starting data block
    int current_block = rootDir[file_index].firstDataBlock;

    // If the file has no data blocks yet, allocate one
    if (current_block == -1) {
        // Find a free block
        for (int i = 0; i < 4096; i++) {
            if (FAT1[i] == -2) { // -2 indicates a free block
                current_block = i;
                rootDir[file_index].firstDataBlock = current_block;
                FAT1[current_block] = -1; // End of file
                FAT2[current_block] = -1;
                break;
            }
        }
        if (current_block == -1) {
            fprintf(stderr, "Error: No free data blocks available.\n");
            return 0; // Disk is full
        }
    } else {
        // Traverse to the correct block
        for (int i = 0; i < block_index_within_file; i++) {
            current_block = FAT1[current_block];
            if (current_block == -1) {
                // Need to allocate a new block
                break;
            }
        }
    }

    while (bytes_to_write > 0) {
        if (current_block == -1) {
            // Need to allocate a new block
            // Find a free block
            int new_block = -1;
            for (int i = 0; i < 4096; i++) {
                if (FAT1[i] == -2) { // -2 indicates a free block
                    new_block = i;
                    FAT1[current_block] = new_block;
                    FAT1[new_block] = -1; // New end of file
                    FAT2[new_block] = -1;
                    break;
                }
            }
            if (new_block == -1) {
                fprintf(stderr, "Warning: Disk is full. Could not allocate new data block.\n");
                break; // No more space to write
            }
            current_block = new_block;
        }

        // Read the existing data block (if any)
        char block_data[BLOCK_SIZE];
        if (block_read(DATA_BLOCKS_START + current_block, block_data) == -1) {
            fprintf(stderr, "Error: Failed to read data block %d.\n", current_block);
            return -1;
        }

        size_t bytes_in_block = BLOCK_SIZE - block_offset;
        size_t bytes_to_copy = bytes_to_write < bytes_in_block ? bytes_to_write : bytes_in_block;

        // Copy data from buf to block_data
        memcpy(block_data + block_offset, (char *)buf + buffer_offset, bytes_to_copy);

        // Write the updated block back to disk
        if (block_write(DATA_BLOCKS_START + current_block, block_data) == -1) {
            fprintf(stderr, "Error: Failed to write data block %d.\n", current_block);
            return -1;
        }

        buffer_offset += bytes_to_copy;
        bytes_to_write -= bytes_to_copy;
        bytes_written += bytes_to_copy;
        file_offset += bytes_to_copy;
        block_offset = 0; // Reset block offset for subsequent blocks

        // Move to next block
        if (bytes_to_write > 0) {
            int next_block = FAT1[current_block];
            if (next_block == -1) {
                // Need to allocate a new block
                int new_block = -1;
                for (int i = 0; i < 4096; i++) {
                    if (FAT1[i] == -2) {
                        new_block = i;
                        FAT1[current_block] = new_block;
                        FAT1[new_block] = -1; // New end of file
                        FAT2[new_block] = -1;
                        break;
                    }
                }
                if (new_block == -1) {
                    fprintf(stderr, "Warning: Disk is full. Could not allocate new data block.\n");
                    break; // No more space to write
                }
                current_block = new_block;
            } else {
                current_block = next_block;
            }
        }
    }

    // Update file descriptor's offset
    file_descriptors[fildes].offset = file_offset;

    // Update file size if necessary
    if (file_offset > rootDir[file_index].sizeInBytes) {
        rootDir[file_index].sizeInBytes = file_offset;
    }

    // Return the number of bytes actually written
    return bytes_written;
}

int fs_get_filesize(int fildes) {
    if (!is_mounted) {
        fprintf(stderr, "Error: File system is not mounted.\n");
        return -1;
    }

    // Validate the file descriptor
    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS || !file_descriptors[fildes].is_open) {
        fprintf(stderr, "Error: Invalid or closed file descriptor.\n");
        return -1;
    }

    int file_index = file_descriptors[fildes].file_index;
    size_t file_size = rootDir[file_index].sizeInBytes;

    return (int)file_size;
}

int fs_lseek(int fildes, off_t offset) {
    if (!is_mounted) {
        fprintf(stderr, "Error: File system is not mounted.\n");
        return -1;
    }

    // Validate the file descriptor
    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS || !file_descriptors[fildes].is_open) {
        fprintf(stderr, "Error: Invalid or closed file descriptor.\n");
        return -1;
    }

    if (offset < 0) {
        fprintf(stderr, "Error: Offset cannot be negative.\n");
        return -1;
    }

    int file_index = file_descriptors[fildes].file_index;
    size_t file_size = rootDir[file_index].sizeInBytes;

    if ((size_t)offset > file_size) {
        fprintf(stderr, "Error: Offset is beyond the end of the file.\n");
        return -1;
    }

    file_descriptors[fildes].offset = (size_t)offset;

    return 0;
}

int fs_truncate(int fildes, off_t length) {
    if (!is_mounted) {
        fprintf(stderr, "Error: File system is not mounted.\n");
        return -1;
    }

    // Validate the file descriptor
    if (fildes < 0 || fildes >= MAX_FILE_DESCRIPTORS || !file_descriptors[fildes].is_open) {
        fprintf(stderr, "Error: Invalid or closed file descriptor.\n");
        return -1;
    }

    if (length < 0) {
        fprintf(stderr, "Error: Length cannot be negative.\n");
        return -1;
    }

    int file_index = file_descriptors[fildes].file_index;
    size_t file_size = rootDir[file_index].sizeInBytes;

    if ((size_t)length > file_size) {
        fprintf(stderr, "Error: Cannot extend file using fs_truncate.\n");
        return -1;
    }

    // If length is equal to current size, nothing to do
    if ((size_t)length == file_size) {
        return 0;
    }

    // If the file pointer is larger than the new length, set it to length
    if (file_descriptors[fildes].offset > (size_t)length) {
        file_descriptors[fildes].offset = (size_t)length;
    }

    // Calculate how many blocks we need to keep
    int blocks_to_keep = (length + BLOCK_SIZE - 1) / BLOCK_SIZE; // Ceiling division

    // Traverse the FAT chain and free blocks beyond the new length
    int current_block = rootDir[file_index].firstDataBlock;
    int prev_block = -1;
    int block_count = 0;

    while (current_block != -1 && block_count < blocks_to_keep) {
        prev_block = current_block;
        current_block = FAT1[current_block];
        block_count++;
    }

    // Now current_block is the block to free and onwards
    while (current_block != -1) {
        int next_block = FAT1[current_block];
        FAT1[current_block] = -2; // Mark as free
        FAT2[current_block] = -2;
        current_block = next_block;
    }

    // Update the FAT to indicate the new end of the file
    if (prev_block != -1) {
        FAT1[prev_block] = -1;
        FAT2[prev_block] = -1;
    } else {
        // If prev_block is -1, that means the file is being truncated to zero length
        rootDir[file_index].firstDataBlock = -1;
    }

    // Update the file size
    rootDir[file_index].sizeInBytes = (size_t)length;

    return 0;
}