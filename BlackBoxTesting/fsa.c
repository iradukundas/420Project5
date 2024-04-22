#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/types.h>
#include <ext2fs/ext2_fs.h>
#include <sys/stat.h>

#define BASE_OFFSET 1024                  // Offset to the superblock from the start of the disk
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block - 1) * block_size)

unsigned int block_size;                  // Block size initialized from superblock

void read_superblock(int fd, struct ext2_super_block *sb);
void print_root_entries(int fd, struct ext2_super_block *sb, FILE* output);

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

    FILE *output = fopen("root_output.txt", "w");
    if (!output) {
        perror("Error opening root output file");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[2], "-root") == 0) {
        print_root_entries(fd, &super, output);
    } else {
        fprintf(stderr, "Invalid option\n");
    }

    fclose(output);
    close(fd);
    return 0;
}

void read_superblock(int fd, struct ext2_super_block *sb) {
    lseek(fd, BASE_OFFSET, SEEK_SET);
    if (read(fd, sb, sizeof(struct ext2_super_block)) != sizeof(struct ext2_super_block)) {
        fprintf(stderr, "Failed to read superblock\n");
        exit(EXIT_FAILURE);
    }
    block_size = 1024 << sb->s_log_block_size;  // Calculate block size dynamically
    fprintf(stderr, "Calculated block size: %u\n", block_size);
}

void print_root_entries(int fd, struct ext2_super_block *sb, FILE* output) {
    fprintf(output, "Block size: %u\n", block_size);
    fprintf(output, "Inode size: %u\n", sb->s_inode_size);

    // Locate the first group descriptor just after the superblock
    struct ext2_group_desc gd;
    int gd_offset = BASE_OFFSET + (block_size == 1024 ? block_size : 2 * block_size);
    fprintf(output, "Group descriptor offset: %d\n", gd_offset);

    lseek(fd, gd_offset, SEEK_SET);
    if (read(fd, &gd, sizeof(gd)) != sizeof(gd)) {
        fprintf(output, "Error reading group descriptor\n");
        return;
    }

    if (gd.bg_inode_table == 0) {
        fprintf(output, "Group descriptor inode table block is zero, which is incorrect.\n");
        return;
    }

    fprintf(output, "Inode table block: %u\n", gd.bg_inode_table);
    off_t inode_table_start = BLOCK_OFFSET(gd.bg_inode_table);
    fprintf(output, "Inode table block start offset: %ld\n", inode_table_start);

    struct ext2_inode inode;
    off_t inode_offset = inode_table_start + (EXT2_ROOT_INO - 1) * sb->s_inode_size;
    fprintf(output, "Root inode offset: %ld\n", inode_offset);
    lseek(fd, inode_offset, SEEK_SET);
    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        fprintf(output, "Error reading root inode\n");
        return;
    }

    fprintf(output, "--Root Directory Entries--\n");
    for (int i = 0; i < EXT2_N_BLOCKS && inode.i_block[i] != 0; i++) {
        fprintf(output, "Root inode block[%d]: %u\n", i, inode.i_block[i]);
        unsigned char block[block_size];
        lseek(fd, BLOCK_OFFSET(inode.i_block[i]), SEEK_SET);
        if (read(fd, block, block_size) != block_size) {
            fprintf(output, "Error reading block %d of root directory\n", i);
            continue;
        }

        int offset = 0;
        while (offset < block_size) {
            struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(block + offset);
            if (entry->inode != 0 && entry->rec_len >= sizeof(struct ext2_dir_entry_2)) {
                char name[EXT2_NAME_LEN + 1];
                memcpy(name, entry->name, entry->name_len);
                name[entry->name_len] = '\0';
                fprintf(output, "Inode: %u, Entry Length: %u, Name Length: %u, File Type: %u, Name: %s\n",
                        entry->inode, entry->rec_len, entry->name_len, entry->file_type, name);

                offset += entry->rec_len;
            } else {
                fprintf(output, "Invalid directory entry: rec_len is zero or inode is zero. Stopping parse.\n");
                break;
            }
        }
    }
}
