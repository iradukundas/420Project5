#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/types.h>
#include <ext2fs/ext2_fs.h>
#include <sys/stat.h>

#define BASE_OFFSET 1024 // Offset to the superblock from the start of the disk
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block) * block_size)

unsigned int block_size; // Block size initialized from superblock

void read_superblock(int fd, struct ext2_super_block *sb);
void print_general_info(int fd, struct ext2_super_block *sb, FILE *output);
void print_group_info(int fd, struct ext2_super_block *sb, FILE *output);
void print_root_entries(int fd, struct ext2_super_block *sb, FILE *output);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk_image_file> <option>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Error opening disk image file");
        exit(EXIT_FAILURE);
    }

    struct ext2_super_block super;
    read_superblock(fd, &super);

    FILE *output = NULL;
    char output_file[256];

    if (strcmp(argv[2], "-info") == 0) {
        snprintf(output_file, sizeof(output_file), "info_output.txt");
        output = fopen(output_file, "w");
        if (output == NULL) {
            perror("Error opening output file");
            close(fd);
            exit(EXIT_FAILURE);
        }
        print_general_info(fd, &super, output);
        print_group_info(fd, &super, output);
    } else if (strcmp(argv[2], "-root") == 0) {
        snprintf(output_file, sizeof(output_file), "root_output.txt");
        output = fopen(output_file, "w");
        if (output == NULL) {
            perror("Error opening output file");
            close(fd);
            exit(EXIT_FAILURE);
        }
        print_root_entries(fd, &super, output);
    } else {
        fprintf(stderr, "Invalid option\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (output != NULL) {
        fclose(output);
    }
    close(fd);
    return 0;
}

void read_superblock(int fd, struct ext2_super_block *sb) {
    lseek(fd, BASE_OFFSET, SEEK_SET);
    if (read(fd, sb, sizeof(struct ext2_super_block)) != sizeof(struct ext2_super_block)) {
        fprintf(stderr, "Failed to read superblock\n");
        exit(EXIT_FAILURE);
    }
    block_size = 1024 << sb->s_log_block_size; // Calculate block size
}

void print_general_info(int fd, struct ext2_super_block *sb, FILE *output) {
    fprintf(output, "--General File System Information--\n");
    fprintf(output, "Block Size in Bytes: %u\n", block_size);
    fprintf(output, "Total Number of Blocks: %u\n", sb->s_blocks_count);
    fprintf(output, "Disk Size in Bytes: %llu\n", (unsigned long long)sb->s_blocks_count * block_size);
    fprintf(output, "Maximum Number of Blocks Per Group: %u\n", sb->s_blocks_per_group);
    fprintf(output, "Inode Size in Bytes: %u\n", sb->s_inode_size);
    fprintf(output, "Number of Inodes Per Group: %u\n", sb->s_inodes_per_group);
    fprintf(output, "Number of Inode Blocks Per Group: %u\n", (sb->s_inodes_per_group * sb->s_inode_size) / block_size);
    fprintf(output, "Number of Groups: %u\n", (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group);
}

void print_group_info(int fd, struct ext2_super_block *sb, FILE *output) {
    struct ext2_group_desc gd;
    unsigned int num_groups = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;

    fprintf(output, "--Individual Group Information--\n");
    for (unsigned int i = 0; i < num_groups; i++) {
        lseek(fd, BLOCK_OFFSET(1) + i * sizeof(struct ext2_group_desc), SEEK_SET);
        if (read(fd, &gd, sizeof(struct ext2_group_desc)) != sizeof(struct ext2_group_desc)) {
            fprintf(stderr, "Error reading group descriptor %u\n", i);
            continue;
        }

        fprintf(output, "-Group %u-\n", i);
        fprintf(output, "Block IDs: %u-%u\n", i * sb->s_blocks_per_group + sb->s_first_data_block,
                (i == num_groups - 1) ? sb->s_blocks_count - 1 : (i + 1) * sb->s_blocks_per_group - 1);
        fprintf(output, "Block Bitmap Block ID: %u\n", gd.bg_block_bitmap);
        fprintf(output, "Inode Bitmap Block ID: %u\n", gd.bg_inode_bitmap);
        fprintf(output, "Inode Table Block ID: %u\n", gd.bg_inode_table);
        fprintf(output, "Number of Free Blocks: %u\n", gd.bg_free_blocks_count);
        fprintf(output, "Number of Free Inodes: %u\n", gd.bg_free_inodes_count);
        fprintf(output, "Number of Directories: %u\n", gd.bg_used_dirs_count);

        // Print free block IDs and free inode IDs
        // Implement the logic to read the block bitmap and inode bitmap
        // and print the free block IDs and free inode IDs as ranges

        fprintf(output, "Free Block IDs: ...\n");
        fprintf(output, "Free Inode IDs: ...\n");
    }
}

void print_root_entries(int fd, struct ext2_super_block *sb, FILE *output) {
    struct ext2_inode inode;
    off_t offset = BLOCK_OFFSET(sb->s_first_data_block) + (EXT2_ROOT_INO - 1) * sb->s_inode_size;
    fprintf(stderr, "Root inode offset: %lu\n", offset);
    
    lseek(fd, offset, SEEK_SET);
    if (read(fd, &inode, sizeof(inode)) != sizeof(inode)) {
        fprintf(stderr, "Error reading root inode\n");
        return;
    }

    fprintf(stderr, "Root inode size: %u\n", inode.i_size);
    fprintf(stderr, "Root inode blocks: %u\n", inode.i_blocks);
    fprintf(stderr, "Root inode block[0]: %u\n", inode.i_block[0]);

    unsigned char block[block_size];
    lseek(fd, BLOCK_OFFSET(inode.i_block[0]), SEEK_SET);
    if (read(fd, block, block_size) != block_size) {
        fprintf(stderr, "Error reading root directory block\n");
        return;
    }

    fprintf(output, "--Root Directory Entries--\n");
    struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)block;
    int count = 0;
    while ((char *)entry < (char *)block + block_size) {
        fprintf(stderr, "Entry offset: %ld\n", (char *)entry - (char *)block);
        fprintf(stderr, "rec_len: %u\n", entry->rec_len);
        fprintf(stderr, "name_len: %u\n", entry->name_len);
        fprintf(stderr, "file_type: %u\n", entry->file_type);
        fprintf(stderr, "inode: %u\n", entry->inode);
        fprintf(stderr, "name: %.*s\n", entry->name_len, entry->name);

        if (entry->rec_len == 0) {
            fprintf(stderr, "Invalid directory entry: rec_len is zero. Skipping entry.\n");
            entry = (struct ext2_dir_entry_2 *)((char *)entry + sizeof(struct ext2_dir_entry_2));
            continue;
        }

        if (entry->inode != 0) {
            fprintf(output, "Inode: %u\n", entry->inode);
            fprintf(output, "Entry Length: %u\n", entry->rec_len);
            fprintf(output, "Name Length: %u\n", entry->name_len);
            fprintf(output, "File Type: %u\n", entry->file_type);
            fprintf(output, "Name: %.*s\n", entry->name_len, entry->name);
            count++;
        }

        entry = (struct ext2_dir_entry_2 *)((char *)entry + entry->rec_len);
    }

    fprintf(output, "Number of entries processed: %d\n", count);
}