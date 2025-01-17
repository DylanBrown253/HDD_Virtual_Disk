# Design Document for Simple FAT-based File System

---

## Volume Layout (Disk Layout)

- The virtual disk has 8,192 blocks, each 4KB in size.
- Block 0: Boot/Super Block.  
  Contains metadata about the file system structure, including the locations of the FAT regions, the root directory, and the data blocks.
- Blocks 100–103 (example): FAT1 Region.  
  The primary File Allocation Table that maps each data block to the next block in a file's chain.  
  `-1` indicates the end of a chain.  
  `-2` indicates a free block.
- Blocks 200–203 (example): FAT2 Region.  
  A secondary copy of the FAT for redundancy.
- Block 300 (example): Root Directory Region.  
  Stores up to 64 file entries. Each entry includes the filename, size, timestamps, and a pointer to the first data block of the file.
- Starting at Block 4096 (example): Data Blocks Region.  
  Contains the actual file data. There are 4,096 data blocks available, allowing a maximum file size of about 16 MB.

Note: The exact block indices for FAT and directory regions are recorded in the super block.

---

## Physical Directory Structure

- The root directory is contained in a single block (e.g., Block 300).
- It holds a fixed-size array of 64 file entries.
  - `isFile`: 1 if in use, 0 if free.
  - `filename[16]`: Null-terminated, up to 15 characters plus the null terminator.
  - `sizeInBytes`
  - `firstDataBlock`: Index of the first data block in the FAT.
  - `timeCreated` and `dateCreated`: Metadata fields.

---
3
## Space Allocation and Free Block Management

- The FAT manages space allocation.
- `-2` in a FAT entry means the block is free.
- `-1` marks the end of a file's chain.
- Positive integers represent the next data block in the file.
- When creating or extending a file, the system searches the FAT for a `-2` (free) block.
- When deleting a file, all of its blocks are returned to `-2` (free).

---

## Logical Directory Structure

- Only a single-level root directory is used.
- The logical directory view (list of files) directly corresponds to the physical root directory block.
- There are no subdirectories, so the logical and physical directory structures are identical.

---

## Relationship Between Logical and Physical Directories

- Since there is only one root directory, the logical and physical directories are the same.
- On mounting the file system, the directory is read from disk into memory.
- On unmounting, any changes are written back to disk.

---

## File Descriptor Management

- Up to 32 file descriptors can be open simultaneously.
- Each file descriptor is maintained in memory only and includes:
  - `is_open`: A boolean indicating if the file is currently open.
  - `file_index`: An index pointing to the file's entry in the root directory.
  - `offset`: The current read/write position within the file.
- File descriptors are not stored on disk.
- They are invalidated when the file system is unmounted or when the file is explicitly closed.

---

## Function Descriptions

- `make_fs(disk_name)`:  
  Creates and initializes a fresh file system on the named disk.  
  Writes the super block, FATs, and root directory.  
  Returns 0 on success, -1 on failure.

- `mount_fs(disk_name)`:  
  Opens the disk and reads the super block, FATs, and root directory into memory.  
  Makes the file system ready for use.  
  Returns 0 on success, -1 on failure.

- `unmount_fs(disk_name)`:  
  Writes all in-memory metadata (FAT, root directory) back to disk and closes it.  
  Closes any open file descriptors.  
  Returns 0 on success, -1 on failure.

- `fs_create(fname)`:  
  Creates a new file in the root directory.  
  Returns 0 on success, -1 if the file already exists, name is too long, or directory is full.

- `fs_delete(fname)`:  
  Deletes the file if it is not open and frees its blocks.  
  Returns 0 on success, -1 on failure.

- `fs_open(fname)`:  
  Opens an existing file and returns a file descriptor (0–31).  
  Offset is set to 0.  
  Returns -1 if the file does not exist or if too many files are open.

- `fs_close(fildes)`:  
  Closes the file descriptor `fildes`.  
  Returns 0 on success, -1 if invalid.

- `fs_read(fildes, buf, nbyte)`:  
  Reads up to `nbyte` bytes from the file at the current offset into `buf`.  
  Returns the number of bytes actually read (may be less if EOF) or -1 on error.

- `fs_write(fildes, buf, nbyte)`:  
  Writes `nbyte` bytes from `buf` into the file, extending it if needed.  
  Returns the number of bytes written (may be less if disk is full) or -1 on error.

- `fs_get_filesize(fildes)`:  
  Returns the size of the file in bytes or -1 if invalid descriptor.

- `fs_lseek(fildes, offset)`:  
  Sets the file descriptor's offset to `offset` (must be within file size).  
  Returns 0 on success, -1 on failure.

- `fs_truncate(fildes, length)`:  
  Truncates the file to `length` bytes, freeing extra blocks.  
  If `offset` is beyond `length`, it adjusts `offset` to `length`.  
  Returns 0 on success, -1 on failure.

---

## Return Values and Parameters

- Most functions return 0 on success and -1 on error.
- `fs_open` returns a non-negative file descriptor on success.
- `fs_read` and `fs_write` return the number of bytes transferred.
- `fs_get_filesize` returns the file size or -1 on error.

Parameters:
- Filenames for create, delete, and open operations.
- File descriptors and offsets/sizes for read, write, seek, and truncate operations.
- Disk names for mount and unmount operations.

---

End of Document
