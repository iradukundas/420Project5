#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ext2fs/ext2fs.h>

#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR 2

// Function declarations
void print_general_info(struct ext2_super_block *sb, FILE *output);
void print_group_info(int fd, struct ext2_super_block *sb, FILE *output);
void print_root_entries(int fd, struct ext2_super_block *sb, FILE *output);
void traverse_directory(int fd, struct ext2_super_block *sb, int inode_num, char *path, FILE *output);
void print_file_content(int fd, struct ext2_super_block *sb, char *path, FILE *output);

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <diskname> <-option> [file_path]\n", argv[0]);
        exit(1);
    }

    char *diskname = argv[1];
    char *option = argv[2];
    FILE *output;
    char output_file[256];

    int fd = open(diskname, O_RDONLY);
    if (fd == -1) {
        perror("Virtual disk open failed");
        exit(1);
    }

    struct ext2_super_block sb;
    if (lseek(fd, 1024, SEEK_SET) != 1024 || read(fd, &sb, sizeof(sb)) != sizeof(sb)) {
        perror("Failed to read superblock");
        exit(1);
    }

    sprintf(output_file, "%s_output.txt", option+1);  // Build the output filename from option
    output = fopen(output_file, "w");
    if (!output) {
        perror("Failed to open output file");
        close(fd);
        exit(1);
    }

    if (strcmp(option, "-info") == 0) {
        print_general_info(&sb, output);
        print_group_info(fd, &sb, output);
    } else if (strcmp(option, "-root") == 0) {
        print_root_entries(fd, &sb, output);
    } else if (strcmp(option, "-traverse") == 0) {
        traverse_directory(fd, &sb, EXT2_ROOT_INO, "/", output);
    } else if (strcmp(option, "-file") == 0 && argc == 4) {
        print_file_content(fd, &sb, argv[3], output);
    } else {
        fprintf(stderr, "Invalid command or insufficient arguments\n");
    }

    if (fclose(output) != 0) {
        perror("Failed to close output file");
    }
    close(fd);

    return 0;
}

void print_general_info(struct ext2_super_block *sb, FILE *output) {
    fprintf(output, "--General File System Information--\n");
    fprintf(output, "Block Size in Bytes: %u\n", 1024 << sb->s_log_block_size);
    fprintf(output, "Total Number of Blocks: %u\n", sb->s_blocks_count);
    fprintf(output, "Disk Size in Bytes: %llu\n", (unsigned long long)sb->s_blocks_count * (1024 << sb->s_log_block_size));
    fprintf(output, "Maximum Number of Blocks Per Group: %u\n", sb->s_blocks_per_group);
    fprintf(output, "Inode Size in Bytes: %u\n", sb->s_inode_size);
    fprintf(output, "Number of Inodes Per Group: %u\n", sb->s_inodes_per_group);
    fprintf(output, "Number of Inode Blocks Per Group: %u\n", (sb->s_inodes_per_group * sb->s_inode_size) / (1024 << sb->s_log_block_size));
    fprintf(output, "Number of Groups: %u\n", (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group);

    if (ferror(output)) {
        perror("Error writing general info to output file");
    }
}

void print_group_info(int fd, struct ext2_super_block *sb, FILE *output) {
    unsigned int num_groups = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;
    struct ext2_group_desc gd;
    unsigned int group_size = sizeof(struct ext2_group_desc);

    fprintf(output, "--Group Information--\n");
    lseek(fd, 1024 + (1 * sb->s_log_block_size), SEEK_SET);  // Seek to the beginning of the group descriptor table

    for (unsigned int i = 0; i < num_groups; i++) {
        if (read(fd, &gd, group_size) != group_size) {
            perror("Error reading group descriptor");
            break;
        }
        fprintf(output, "Group %u: \n", i);
        fprintf(output, "  Block Bitmap Block ID: %u\n", gd.bg_block_bitmap);
        fprintf(output, "  Inode Bitmap Block ID: %u\n", gd.bg_inode_bitmap);
        fprintf(output, "  Inode Table Block ID: %u\n", gd.bg_inode_table);
        fprintf(output, "  Free Blocks Count: %u\n", gd.bg_free_blocks_count);
        fprintf(output, "  Free Inodes Count: %u\n", gd.bg_free_inodes_count);
        fprintf(output, "  Used Directories Count: %u\n", gd.bg_used_dirs_count);

        if (ferror(output)) {
            perror("Error writing group info to output file");
            break;
        }
    }
}

void print_root_entries(int fd, struct ext2_super_block *sb, FILE *output) {
    struct ext2_inode inode;
    struct ext2_dir_entry_2 *entry;
    unsigned int inode_table_block = sb->s_first_data_block + 1; // Inode table block immediately follows the superblock and group descriptor
    unsigned int size = sizeof(struct ext2_inode);

    lseek(fd, inode_table_block * sb->s_log_block_size + 2 * size, SEEK_SET); // Inode 2 is the root inode
    if (read(fd, &inode, size) != size) {
        perror("Error reading root inode");
        return;
    }

    char block[sb->s_log_block_size];
    lseek(fd, inode.i_block[0] * sb->s_log_block_size, SEEK_SET);
    if (read(fd, block, sb->s_log_block_size) != sb->s_log_block_size) {
        perror("Error reading root directory block");
        return;
    }

    fprintf(output, "--Root Directory Entries--\n");
    for (int offset = 0; offset < sb->s_log_block_size; offset += entry->rec_len) {
        entry = (struct ext2_dir_entry_2 *)(block + offset);
        fprintf(output, "  Inode: %u, Rec_len: %u, Name_len: %u, Type: %u, Name: %.*s\n",
                entry->inode, entry->rec_len, entry->name_len, entry->file_type, entry->name_len, entry->name);

        if (ferror(output)) {
            perror("Error writing root entries to output file");
            break;
        }
    }
}

void traverse_directory(int fd, struct ext2_super_block *sb, int inode_num, char *path, FILE *output) {
    struct ext2_inode inode;
    struct ext2_dir_entry_2 *entry;
    char block[sb->s_log_block_size];

    lseek(fd, ((inode_num - 1) / sb->s_inodes_per_group) * sb->s_log_block_size + ((inode_num - 1) % sb->s_inodes_per_group) * sizeof(struct ext2_inode), SEEK_SET);
    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        perror("Error reading inode");
        return;
    }

    for (int i = 0; i < 12; i++) {  // Only direct blocks are considered
        if (inode.i_block[i] == 0) continue;
        lseek(fd, inode.i_block[i] * sb->s_log_block_size, SEEK_SET);
        if (read(fd, block, sb->s_log_block_size) != sb->s_log_block_size) {
            perror("Error reading directory block");
            continue;
        }

        for (int offset = 0; offset < sb->s_log_block_size; offset += entry->rec_len) {
            entry = (struct ext2_dir_entry_2 *)(block + offset);
            if (entry->inode != 0) {
                fprintf(output, "%s/%.*s\n", path, entry->name_len, entry->name);
                if (ferror(output)) {
                    perror("Error writing directory entry to output file");
                    return;
                }

                if (entry->file_type == EXT2_FT_DIR && entry->name[0] != '.') { // Skip '.' and '..'
                    char new_path[1024];
                    snprintf(new_path, sizeof(new_path), "%s/%.*s", path, entry->name_len, entry->name);
                    traverse_directory(fd, sb, entry->inode, new_path, output);
                }
            }
        }
    }
}

void print_file_content(int fd, struct ext2_super_block *sb, char *path, FILE *output) {
    // Simplification for brevity: Assume path directly leads to an inode number for demonstration
    struct ext2_inode inode;
    char block[sb->s_log_block_size];
    int inode_num = atoi(path);  // Mock example, replace with actual path resolution logic

    lseek(fd, ((inode_num - 1) / sb->s_inodes_per_group) * sb->s_log_block_size + ((inode_num - 1) % sb->s_inodes_per_group) * sizeof(struct ext2_inode), SEEK_SET);
    if (read(fd, &inode, sizeof(struct ext2_inode)) != sizeof(struct ext2_inode)) {
        perror("Error reading inode");
        return;
    }

    for (int i = 0; i < 12; i++) {  // Only direct blocks are considered
        if (inode.i_block[i] == 0) continue;
        lseek(fd, inode.i_block[i] * sb->s_log_block_size, SEEK_SET);
        ssize_t bytes_read = read(fd, block, sb->s_log_block_size);
        if (bytes_read < 0) {
            perror("Error reading file block");
            continue;
        }
        if (fwrite(block, 1, bytes_read, output) != bytes_read) {
            perror("Error writing file content to output file");
            return;
        }
    }
}