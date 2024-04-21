#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/types.h>
#include <ext2fs/ext2_fs.h>
#include <sys/stat.h>
#include <endian.h>

#define BASE_OFFSET 1024                   // Offset to the superblock from the start of the disk
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block - 1) * block_size)

unsigned int block_size = 1024;            // Default block size is 1024 bytes

void read_superblock(int fd, struct ext2_super_block *sb);
void traverse_directory(int fd, struct ext2_super_block *sb, struct ext2_inode *inode, const char *path_prefix);
void print_directory_contents(int fd, struct ext2_super_block *sb, struct ext2_inode *inode, const char *path_prefix);
void print_file_content(int fd, struct ext2_super_block *sb, char *path, FILE *output);

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

    if (strcmp(argv[2], "-traverse") == 0) {
        struct ext2_inode root_inode;
        lseek(fd, BASE_OFFSET + super.s_inode_size * (EXT2_ROOT_INO - 1), SEEK_SET);
        read(fd, &root_inode, sizeof(struct ext2_inode));
        printf("Debug: Root inode mode: %o (expected directory mode: %o), raw mode: %x\n", root_inode.i_mode, S_IFDIR, root_inode.i_mode);
        traverse_directory(fd, &super, &root_inode, "/");
    } else {
        fprintf(stderr, "Invalid option\n");
        close(fd);
        exit(EXIT_FAILURE);
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
    block_size = 1024 << sb->s_log_block_size;
    printf("Debug: Superblock - Magic: %x, Inodes count: %u, Block size: %u\n", sb->s_magic, sb->s_inodes_count, block_size);
}

void traverse_directory(int fd, struct ext2_super_block *sb, struct ext2_inode *inode, const char *path_prefix) {
    if ((inode->i_mode & S_IFMT) != S_IFDIR) {
        fprintf(stderr, "Not a directory: %s, mode: %o (mask: %o), raw mode: %x\n", path_prefix, inode->i_mode, S_IFMT, inode->i_mode);
        return;
    }
    print_directory_contents(fd, sb, inode, path_prefix);
}

void print_directory_contents(int fd, struct ext2_super_block *sb, struct ext2_inode *inode, const char *path_prefix) {
    unsigned char block[block_size];
    struct ext2_dir_entry_2 *entry;
    int i = 0;

    while (i < 12 && inode->i_block[i]) {
        lseek(fd, BLOCK_OFFSET(inode->i_block[i]), SEEK_SET);
        read(fd, block, block_size);

        for (entry = (struct ext2_dir_entry_2 *)block; (char *)entry < (char *)block + block_size; entry = (struct ext2_dir_entry_2 *)((char *)entry + entry->rec_len)) {
            char file_name[EXT2_NAME_LEN + 1];
            memcpy(file_name, entry->name, entry->name_len);
            file_name[entry->name_len] = '\0';

            char new_path[1024];
            snprintf(new_path, sizeof(new_path), "%s/%s", path_prefix, file_name);
            printf("%s (type %d)\n", new_path, entry->file_type);

            if (entry->file_type == EXT2_FT_DIR && strcmp(file_name, ".") != 0 && strcmp(file_name, "..") != 0) {
                struct ext2_inode new_inode;
                lseek(fd, BASE_OFFSET + sb->s_inode_size * (entry->inode - 1), SEEK_SET);
                read(fd, &new_inode, sizeof(struct ext2_inode));
                print_directory_contents(fd, sb, &new_inode, new_path);
            }
        }
        i++;
    }
}
