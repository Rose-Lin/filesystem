#include "filesystem.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
  // FILE *disk = fopen("DISKDIR", "rb+");
  // inode *block = malloc(BLOCKSIZE);
  // fseek(disk, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + 2 * sizeof(inode), SEEK_SET);
  // fread(block, sizeof(inode), 1, disk);

  setup();
  printf("%s\n", "-------------end set up--------");
  int num = 0;
  f_mount("DISKDIR", "", &num);
  printf("%s\n", "----------end f_mount---------");
  f_opendir("");
  //f_open("/a/d/s/4.txt", READ, NULL);
  // print_file_table();
  //printf("%s\n", "-----------end f_open1---------");
  //f_open("/user/user.txt", READ, NULL);
  //printf("%s\n", "------------end f_open2----------");
  int fd = f_open("/user/test.txt", APPEND, NULL);
  //printf("%s\n", "------------end f_open3----------");
  // int fd2 = f_open("/user/1.txt", WRITE, NULL);
  // int fd3 = f_open("/user/2.txt", READ, NULL);
  //printf("fd: %d\n", fd);
  //printf("fd2: %d\n", fd2);
  //printf("fd3: %d\n", fd3);
  //print_file_table();
  // char* content = "1";
  printf("%s\n", "-----------------done open files-------------");
  char* large = "123456789roseROSEsarahSARAHbootbootbootbootboot";
  printf("total added: %ld\n", strlen(large)*100);
  // f_write(content, 1, 1, fd);
  // printf("Contents of block2: %s\n", (char*) block);
  // inode* node = get_inode(2);
  // print_inode(node);
  // free(node);
  // printf("%s\n", "----------done writing 1----------");
  // printf("large data size: %d\n", strlen(large));
  f_write(large, strlen(large), 500, fd);
  // f_close(fd);
  //
  void *block = get_data_block(2);
  // fd = f_open("/user/test.txt", READ, NULL);
  // f_rewind(fd);
  // char *large_file = malloc(strlen(large) * 30);
  // f_read(large_file, strlen(large), 30, fd);
  // printf("File contents: %s\n", large_file);
  // block = get_data_block(2);
  printf("Contents of block2: %s\n", (char*) block);
  block = get_data_block(3);
  printf("Contents of block3: %s\n", (char*) block);
  // inode* node2 = get_inode(2);
  // print_inode(node2);
  // free(node2);
  // inode* node1 = get_inode(1);
  // print_inode(node1);
  // free(node1);
  // f_close(fd2);
  // f_remove("/user/1.txt");
  // print_file_table();
  //directory_entry* rose = f_mkdir("/user/rose");
  // directory_entry* roselevel3 = f_mkdir("/user/rose/documents");
  // directory_entry* sarah = f_mkdir("/user/sarah");
  // directory_entry* sarahlevel3 = f_mkdir("/user/sarah/photos");

  // f_stat("/user/test.txt", stats);
  // stat *stats = malloc(sizeof(stat));
  // printf("stats values %d, %d\n", stats->size, stats->inode_index);
  // f_write(text, strlen(text), 1, fd);
  // void* buffer = malloc(200);
  // f_read(buffer, 5, 1, 1);
  // inode *i = get_inode(2);
  // printf("%s\n", (char*) buffer);
  //free(rose);
  f_unmount(num);
  shutdown();
}
