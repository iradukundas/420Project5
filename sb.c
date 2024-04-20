#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>

int main(int argc, char *argv[]) {

  int i, rv, fd;
  unsigned char byte;
  char *sb1 = NULL;
  struct ext2_super_block *sb2 = NULL;
  
  if (argc != 2) {
    fprintf (stderr, "%s: Usage: ./sb disk_image_file\n", "sb");
    exit(1);
  }
  
  fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    perror("disk_image_file open failed");
    exit(1);
  }
  
  /*skip the boot info - the first 1024 bytes - using the lseek*/
  if (lseek(fd, 1024, SEEK_CUR) != 1024) {
    perror("File seek failed");
    exit(1);
  }
  
  sb1 = malloc(1024);
  sb2 = malloc(sizeof(struct ext2_super_block));

  if (sb1 == NULL || sb2 == NULL) {
    fprintf (stderr, "%s: Error in malloc\n", "sb");
    exit(1);
  }
  
  /*read the superblock byte by byte, print each byte, and store in sb1*/  
  for (i = 0; i < 1024; i++) {
    rv = read(fd, &byte, 1);
    if (rv == -1) {
      perror("File read failed");
      exit(1);
    }
    if (rv == 1) {
      printf("byte[%d]: 0x%02X\n", i+1024, byte);
      sb1[i] = byte;
    }    
  }

  printf ("Total Number of Inodes: %u\n", *(unsigned int *)sb1);
  printf ("Number of Free Inodes: %u\n", *(unsigned int *)(sb1+16));
  

  /*set the file offset to byte 1024 again*/
  if (lseek(fd, 1024, SEEK_SET) != 1024) {
    perror("File seek failed");
    exit(1);
  }
  
  /*read the whole superblock and load into the ext2_super_block struct*/
  /*assumes the struct fields are laid out in the order they are defined*/
  rv = read(fd, sb2, sizeof(struct ext2_super_block));
  if (rv == -1) {
    perror("File read failed");
    exit(1);
  }
  if (rv == 1024) {
    printf ("Total Number of Inodes: %u\n", sb2->s_inodes_count);
    printf ("Number of Free Inodes: %u\n", sb2->s_free_inodes_count);
  }    
  
  free(sb1);
  free(sb2);
  close(fd);
  
  return 0;
}
