#include "fs_management.h"
#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// ANSI Escape Codes for Formatting
#define RESET       "\x1B[0m"
#define BOLD        "\x1B[1m"
#define RED         "\x1B[31m"
#define GREEN       "\x1B[32m"
#define BLUE        "\x1B[34m"
#define CYAN        "\x1B[36m"

// Print the initial menu (before disk is ready)
void print_initial_menu() {
    printf("\n" BOLD BLUE "========================================\n");
    printf("         File System Setup Menu\n");
    printf("========================================" RESET "\n");
    printf("1. Create Disk\n");
    printf("2. Open (Mount) Disk\n");
    printf("0. Quit\n");
    printf("Enter your choice: ");
}

// Print the file system menu (after disk is mounted)
void print_fs_menu() {
    printf("\n" BOLD CYAN "========================================\n");
    printf("         Simple File System Menu\n");
    printf("========================================" RESET "\n");
    printf(" 3. Create File\n");
    printf(" 4. Open File\n");
    printf(" 5. Write to File\n");
    printf(" 6. Read from File\n");
    printf(" 7. Copy File\n");
    printf(" 8. Delete File\n");
    printf(" 9. Unmount Disk\n");
    printf("10. Close File\n");
    printf("11. Seek in File\n");
    printf("12. Get File Size\n");
    printf("13. Truncate File\n");
    printf(" 0. Exit\n");
    printf("Enter your choice: ");
}

int main() {
    char disk_name[50];
    char filename[50];
    char copy_filename[50];
    int fd, choice;
    int is_mounted = 0;

    // Initial loop: create or open disk first
    while (1) {
        print_initial_menu();
        if (scanf("%d", &choice) != 1) {
            printf(RED "Invalid input. Please enter a number.\n" RESET);
            while (getchar() != '\n');
            continue;
        }
        getchar(); // consume newline

        if (choice == 0) {
            printf("Exiting...\n");
            return 0;
        } else if (choice == 1) {
            printf("Enter disk name to create: ");
            scanf("%s", disk_name);
            if (make_fs(disk_name) == 0) {
                printf(GREEN "Disk '%s' created successfully!\n" RESET, disk_name);
                // Try mounting it
                if (mount_fs(disk_name) == 0) {
                    is_mounted = 1;
                    printf(GREEN "Disk '%s' mounted successfully!\n" RESET, disk_name);
                    break;
                } else {
                    printf(RED "Failed to mount disk '%s'.\n" RESET, disk_name);
                }
            } else {
                printf(RED "Failed to create disk '%s'.\n" RESET, disk_name);
            }
        } else if (choice == 2) {
            printf("Enter disk name to open: ");
            scanf("%s", disk_name);
            if (mount_fs(disk_name) == 0) {
                is_mounted = 1;
                printf(GREEN "Disk '%s' mounted successfully!\n" RESET, disk_name);
                break;
            } else {
                printf(RED "Failed to mount disk '%s'. Check if it exists.\n" RESET, disk_name);
            }
        } else {
            printf(RED "Invalid choice. Try again.\n" RESET);
        }
    }

    // Once disk is mounted, show the full filesystem menu
    while (1) {
        print_fs_menu();
        if (scanf("%d", &choice) != 1) {
            printf(RED "Invalid input. Please enter a number.\n" RESET);
            while (getchar() != '\n');
            continue;
        }
        getchar(); // consume newline

        switch (choice) {
        case 3:
            if (!is_mounted) {
                printf(RED "Disk is not mounted.\n" RESET);
                break;
            }
            printf("Enter filename to create: ");
            scanf("%s", filename);
            if (fs_create(filename) == 0) {
                printf(GREEN "File '%s' created successfully!\n" RESET, filename);
            } else {
                printf(RED "Failed to create file '%s'.\n" RESET, filename);
            }
            break;

        case 4:
            if (!is_mounted) {
                printf(RED "Disk is not mounted.\n" RESET);
                break;
            }
            printf("Enter filename to open: ");
            scanf("%s", filename);
            fd = fs_open(filename);
            if (fd >= 0) {
                printf(GREEN "File '%s' opened successfully with descriptor %d.\n" RESET, filename, fd);
            } else {
                printf(RED "Failed to open file '%s'.\n" RESET, filename);
            }
            break;

        case 5:
            if (!is_mounted) {
                printf(RED "Disk is not mounted.\n" RESET);
                break;
            }
            printf("Enter file descriptor to write to: ");
            if (scanf("%d", &fd) != 1) {
                printf(RED "Invalid input.\n" RESET);
                break;
            }
            getchar();
            printf("Enter data to write: ");
            {
                char write_data[4096] = {0};
                fgets(write_data, sizeof(write_data), stdin);
                int bytes_written = fs_write(fd, write_data, strlen(write_data));
                if (bytes_written >= 0) {
                    printf(GREEN "Successfully wrote %d bytes.\n" RESET, bytes_written);
                } else {
                    printf(RED "Failed to write to file descriptor %d.\n" RESET, fd);
                }
            }
            break;

        case 6:
            if (!is_mounted) {
                printf(RED "Disk is not mounted.\n" RESET);
                break;
            }
            printf("Enter file descriptor to read from: ");
            if (scanf("%d", &fd) != 1) {
                printf(RED "Invalid input.\n" RESET);
                break;
            }
            printf("Enter number of bytes to read: ");
            {
                int nbytes;
                if (scanf("%d", &nbytes) != 1 || nbytes <= 0) {
                    printf(RED "Invalid input. Positive number required.\n" RESET);
                    break;
                }
                char read_data[4096] = {0};
                int bytes_read = fs_read(fd, read_data, nbytes);
                if (bytes_read >= 0) {
                    read_data[bytes_read] = '\0';
                    printf(GREEN "Read %d bytes: %s\n" RESET, bytes_read, read_data);
                } else {
                    printf(RED "Failed to read from file descriptor %d.\n" RESET, fd);
                }
            }
            break;

        case 7:
            if (!is_mounted) {
                printf(RED "Disk is not mounted.\n" RESET);
                break;
            }
            printf("Enter source filename: ");
            scanf("%s", filename);
            printf("Enter destination filename: ");
            scanf("%s", copy_filename);
            {
                int src_fd = fs_open(filename);
                if (src_fd < 0) {
                    printf(RED "Failed to open source file '%s'.\n" RESET, filename);
                    break;
                }

                if (fs_create(copy_filename) != 0) {
                    printf(RED "Failed to create destination file '%s'.\n" RESET, copy_filename);
                    fs_close(src_fd);
                    break;
                }

                int dest_fd = fs_open(copy_filename);
                if (dest_fd < 0) {
                    printf(RED "Failed to open destination file '%s'.\n" RESET, copy_filename);
                    fs_close(src_fd);
                    break;
                }

                char buffer[BLOCK_SIZE];
                int bytes;
                while ((bytes = fs_read(src_fd, buffer, BLOCK_SIZE)) > 0) {
                    if (fs_write(dest_fd, buffer, bytes) != bytes) {
                        printf(RED "Error writing to destination file.\n" RESET);
                        break;
                    }
                }

                fs_close(src_fd);
                fs_close(dest_fd);
                printf(GREEN "File '%s' copied to '%s'.\n" RESET, filename, copy_filename);
            }
            break;

        case 8:
            if (!is_mounted) {
                printf(RED "Disk is not mounted.\n" RESET);
                break;
            }
            printf("Enter filename to delete: ");
            scanf("%s", filename);
            if (fs_delete(filename) == 0) {
                printf(GREEN "File '%s' deleted successfully!\n" RESET, filename);
            } else {
                printf(RED "Failed to delete file '%s'.\n" RESET, filename);
            }
            break;

        case 9:
            if (!is_mounted) {
                printf(RED "Disk is not mounted.\n" RESET);
                break;
            }
            if (unmount_fs(disk_name) == 0) {
                is_mounted = 0;
                printf(GREEN "Disk '%s' unmounted successfully!\n" RESET, disk_name);
                while (getchar() != '\n');
                main();
                return 0;
            } else {
                printf(RED "Failed to unmount disk '%s'.\n" RESET, disk_name);
            }
            break;

        case 10: // Close file
            if (!is_mounted) {
                printf(RED "Disk is not mounted.\n" RESET);
                break;
            }
            printf("Enter file descriptor to close: ");
            if (scanf("%d", &fd) != 1) {
                printf(RED "Invalid input.\n" RESET);
                break;
            }
            if (fs_close(fd) == 0) {
                printf(GREEN "File descriptor %d closed successfully.\n" RESET, fd);
            } else {
                printf(RED "Failed to close file descriptor %d.\n" RESET, fd);
            }
            break;

        case 11: // Seek in file
            if (!is_mounted) {
                printf(RED "Disk is not mounted.\n" RESET);
                break;
            }
            printf("Enter file descriptor to seek in: ");
            if (scanf("%d", &fd) != 1) {
                printf(RED "Invalid input.\n" RESET);
                break;
            }
            printf("Enter offset to seek to: ");
            {
                int offset;
                if (scanf("%d", &offset) != 1) {
                    printf(RED "Invalid offset.\n" RESET);
                    break;
                }
                if (fs_lseek(fd, offset) == 0) {
                    printf(GREEN "Seeked to offset %d in file descriptor %d.\n" RESET, offset, fd);
                } else {
                    printf(RED "Failed to seek in file descriptor %d.\n" RESET, fd);
                }
            }
            break;

        case 12: // Get file size
            if (!is_mounted) {
                printf(RED "Disk is not mounted.\n" RESET);
                break;
            }
            printf("Enter file descriptor to get size from: ");
            if (scanf("%d", &fd) != 1) {
                printf(RED "Invalid input.\n" RESET);
                break;
            }
            {
                int size = fs_get_filesize(fd);
                if (size >= 0) {
                    printf(GREEN "File size: %d bytes.\n" RESET, size);
                } else {
                    printf(RED "Failed to get file size for descriptor %d.\n" RESET, fd);
                }
            }
            break;

        case 13: // Truncate file
            if (!is_mounted) {
                printf(RED "Disk is not mounted.\n" RESET);
                break;
            }
            printf("Enter file descriptor to truncate: ");
            if (scanf("%d", &fd) != 1) {
                printf(RED "Invalid input.\n" RESET);
                break;
            }
            printf("Enter length to truncate to: ");
            {
                int length;
                if (scanf("%d", &length) != 1 || length < 0) {
                    printf(RED "Invalid length.\n" RESET);
                    break;
                }
                if (fs_truncate(fd, length) == 0) {
                    printf(GREEN "File truncated to %d bytes.\n" RESET, length);
                } else {
                    printf(RED "Failed to truncate file descriptor %d.\n" RESET, fd);
                }
            }
            break;

        case 0:
            printf("Exiting...\n");
            exit(0);

        default:
            printf(RED "Invalid choice. Try again.\n" RESET);
        }
    }

    return 0;
}
