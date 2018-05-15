#include <math.h>
#include "filesystem.h"
#include "boolean.h"
#include "stdlib.h"

//global variables and arrays
mount_table_entry* mounted_disks[MOUNTTABLESIZE];
file_table_entry* file_table[FILETABLESIZE];
mount_table_entry *current_mounted_disk;
inode *root_inode;
int table_freehead = 0;
directory_entry* root_dir_entry = NULL;

//the shell must call this method to set up the global variables and structures
boolean setup() {
    for (int i = 0; i < MOUNTTABLESIZE; i++) {
        mounted_disks[i] = (mount_table_entry *) malloc(sizeof(mount_table_entry));
        memset(mounted_disks[i], 0, sizeof(mount_table_entry));
        mounted_disks[i]->free_spot = TRUE;
        mounted_disks[i]->superblock1 = malloc(sizeof(superblock));
        memset(mounted_disks[i]->superblock1, 0, sizeof(superblock));
    }

    for (int j = 0; j < FILETABLESIZE; j++) {
        file_table[j] = malloc(sizeof(file_table_entry));
        memset(file_table[j], 0, sizeof(file_table_entry));
        file_table[j]->file_inode = malloc(sizeof(inode));
        memset(file_table[j]->file_inode, 0, sizeof(inode));
        file_table[j]->free_file = TRUE;
    }

    root_dir_entry = malloc(sizeof(directory_entry));
    memset(root_dir_entry, 0, sizeof(directory_entry));

    return TRUE;
}

//do this upon shell exit to ensure no memory leaks
boolean shutdown() {
    for (int i = 0; i < MOUNTTABLESIZE; i++) {
        free(mounted_disks[i]->superblock1);
        free(mounted_disks[i]);
    }

    for (int j = 0; j < FILETABLESIZE; j++) {
        if(file_table[j]->file_inode != NULL){
          free(file_table[j]->file_inode);
        }
        free(file_table[j]);
    }
    // if(root_dir_entry != NULL){
    //   free(root_dir_entry);
    // }
    return TRUE;
}

//when free in shutdown() causes direct loss
//This way it truns into indirect. So I guess it is better? TODO.
void free_root_dir(){
    free(root_dir_entry);
}

int first_free_location_in_mount() {
    int index = -1;
    for (int i = 0; i < MOUNTTABLESIZE; i++) {
        if (mounted_disks[i]->free_spot == TRUE) {
            index = i;
            break;
        }
    }

    return index;
}

int desired_free_location_in_table(int location_sought) {
    int num_locations = 1;
    int index = -1;
    for (int i = 0; i < FILETABLESIZE; i++) {
        if (file_table[i]->free_file == TRUE) {
            if (num_locations < location_sought) {
                num_locations++;
            } else {
                index = i;
                break;
            }
        }
    }

    return index;
}

file_table_entry *get_table_entry(int index) {
    if (file_table[index]->free_file == TRUE || index < 0 || index > FILETABLESIZE) {
        return NULL;
    }
    return file_table[index];
}

mount_table_entry *get_mount_table_entry(int index) {
    return mounted_disks[index];
}

int first_free_inode() {
    return current_mounted_disk->superblock1->free_inode;
}

int get_fd_from_inode_value(int inode_index) {
    int val = -1;
    for (int i = 0; i < FILETABLESIZE; i++) {
        if (file_table[i]->free_file == FALSE) {
            if (file_table[i]->file_inode->inode_index == inode_index) {
                val = i;
                break;
            }
        }
    }

    return val;
}

boolean if_is_file(int inode_index) {
    inode *node = get_inode(inode_index);
    int type = node->type;
    free(node);
    return (type == REG);
}

directory_entry get_last_directory_entry(int fd) {
    int directory_size = file_table[fd]->file_inode->size;
    int last_block_index = directory_size / BLOCKSIZE;
    int block_location = last_block_index % BLOCKSIZE;
    int directory_location = block_location / sizeof(directory_entry) - 1;
    inode *file_inode = file_table[fd]->file_inode;
    int data_region_index = -1;
    void *last_block = get_block_from_index(last_block_index, file_inode, &data_region_index);
    directory_entry *array = last_block;
    directory_entry value = array[directory_location];
    free_data_block(last_block);
    return value;
}

boolean f_mount(char *disk_img, char *mounting_point, int *mount_table_index) {
    //open the disk
    //TODO: check that the disk image actually exists...
    //TODO: actually do something with mounting_point value passed in...location to mount (NOT ALWAYS ROOT!)
    int free_index = -1;
    //look for empty index into fmount table
    for (int i = 0; i < MOUNTTABLESIZE; i++) {
        if (mounted_disks[i]->free_spot == TRUE) {
            free_index = i;
            break;
        }
    }
    if (free_index != -1) {
        *mount_table_index = free_index;
        FILE *file_to_mount = fopen(disk_img, "rb+");
        if (file_to_mount == NULL) {
            printf("%s\n", "Open Disk failed. Please check that you have run format to create a file named DISK.");
            return FALSE;
        }
        fseek(file_to_mount, 0L, SEEK_END);
        int disksize = ftell(file_to_mount);
        if (disksize <= 0) {
            printf("%s\n", "Disk invalid size. ");
            return FALSE;
        }
        //skip the boot block
        mounted_disks[free_index]->free_spot = FALSE;
        mounted_disks[free_index]->disk_image_ptr = file_to_mount;
        fseek(file_to_mount, SIZEOFBOOTBLOCK, SEEK_SET); //place the file pointer at the superblock
        fread(mounted_disks[free_index]->superblock1, sizeof(superblock), 1, file_to_mount);
        current_mounted_disk = mounted_disks[free_index];

        //for testing: find data block
        superblock *sp = mounted_disks[free_index]->superblock1;
        fseek(file_to_mount, (sp->data_offset) * sp->size + SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK, SEEK_SET);
        void *data_content = malloc(sizeof(char) * sp->size);
        memset(data_content, 0, sizeof(char) * sp->size);
        fread(data_content, sp->size, 1, file_to_mount);
        free(data_content);

        //TODO: figure out what to do with inodes and pointing to them (remaining values in the structs)
        return TRUE;
    } else {
        return FALSE; //a spot was not found
    }
    return FALSE;
}

boolean f_unmount(int mid) {
    if (mid >= 0 && mid < MOUNTTABLESIZE) {
        if (mounted_disks[mid]->free_spot == FALSE) {
            mounted_disks[mid]->free_spot = TRUE;
            fclose(mounted_disks[mid]->disk_image_ptr); //TODO: ADD BACK!!
            // free(mounted_disks[mid]->mounted_inode); //not doing anything with this, yet...
            // free(mounted_disks[mid]->superblock1); //free this in the shutdown() method...
            return TRUE;
        } else {
            return FALSE;
        }
    } else {
        return FALSE;
    }
}

/* f_open() method */ //TODO: add permissions functionality //TODO: add access fulctonality with timing...
int f_open(char* filepath, int access, permission_value *permissions) {
    //get the filename and the path seperately
    char *filename = NULL;
    char *path = malloc(strlen(filepath));
    memset(path, 0, strlen(filepath));
    char path_copy[strlen(filepath) + 1];
    char copy[strlen(filepath) + 1];
    strcpy(path_copy, filepath);
    strcpy(copy, filepath);
    char *s = "/";
    //calculate the level of depth of dir
    char *token = strtok(copy, s);
    int count = 0;
    while (token != NULL) {
        count++;
        token = strtok(NULL, s);
    }
    // printf("count : %d\n", count);
    filename = strtok(path_copy, s);
    while (count > 1) {
        count--;
        path = strcat(path, "/");
        path = strcat(path, filename);
        filename = strtok(NULL, s);
    }
    // printf("path: %s\n", path);
    printf("filename: %s\n", filename);
    directory_entry *dir = f_opendir(path);
    if (dir == NULL) {
        printf("%s\n", "directory does not exist");
        free(path);
        return EXITFAILURE;
      } else {
        //directory exits, need to check if the file exits
        // printf("%s\n", "directory exits. GOOD news.");
        // int dir_node_index = dir->inode_index;
        // printf("dir_index: %d\n", dir_node_index);
        // printf("dir_name: %s\n", dir->filename);
        int dir_fd = get_fd_from_inode_value(dir->inode_index);
        inode *dir_node = file_table[dir_fd]->file_inode;
        int parent_fd = dir_fd;
        // inode *dir_node = get_inode(dir_node_index);
        directory_entry *entry = NULL;
        //go into the dir_entry to find the inode of the file
        file_table[parent_fd]->byte_offset = 0;
        for (int i = 0; i < dir_node->size; i += sizeof(directory_entry)) {
            entry = f_readdir(parent_fd);
            if (entry == NULL) {
                free(entry);
                break;
            }
            if (strcmp(entry->filename, filename) == 0 ) {
                if(if_is_file(entry->inode_index) == TRUE){
                  // printf("%s found, and is a REG file\n", entry->filename);
                  file_table_entry *file_entry = file_table[table_freehead];
                  file_entry->free_file = FALSE;
                  free(file_entry->file_inode);
                  inode *file_inode = get_inode(entry->inode_index);
                  // set_permissions(file_inode->permission, permissions);
                  // update_single_inode_ondisk(file_inode, file_inode->inode_index);
                  file_entry->file_inode = file_inode;
                  file_entry->byte_offset = 0;
                  file_entry->access = access;
                  free(path);
                  int fd = table_freehead;
                  table_freehead = find_next_freehead();
                  free(entry);
                  // free(dir_node);
                  free(dir);
                  return fd;
                }
            }
            free(entry);
          }

        //file was not found...
        if (access == READ) {
            printf("%s\n", "file does not found");
            free(path);
            free(dir);
            // free(dir_node);
            // print_superblock(current_mounted_disk->superblock1);
            return EXITFAILURE;
        } else { //need to create this file!!
            printf("%s\n", "need to create this new file--------------!!!");
            //creating new directory entry
            directory_entry *newfile = malloc(sizeof(directory_entry));
            memset(newfile, 0, sizeof(directory_entry));
            strcpy(newfile->filename, filename);

            int new_inode_index = current_mounted_disk->superblock1->free_inode;
            if (new_inode_index == -1) {
                printf("%s\n", "not enough space to create new folder, reach the max");
                free(newfile);
                // free(dir_node);
                return EXITFAILURE;
            }
            newfile->inode_index = new_inode_index;

            //at this point, newfile has the directory name and the directory inode value...
            //need to write newfile's directory entry to disk and create inode for newfile
            inode *new_inode = get_inode(new_inode_index); //this is inode for the file
            current_mounted_disk->superblock1->free_inode = new_inode->next_inode;

            // printf("PRINTING SUPERBLOCK STATE AND NEW FILE INODE STATE HERE\n");
            // print_superblock(current_mounted_disk->superblock1);
            // print_inode(new_inode);

            //get the datablock to write the newfile directory entry into
            void *dir_data = get_data_block(dir_node->last_block_index);
            if (dir_node->size == BLOCKSIZE * (dir_node->size / BLOCKSIZE)) {
                //request new blocks
                printf("%s\n", "requesting new blocks for directory");
                int total_block = dir_node->size/BLOCKSIZE + 1;
                int new_block_index = find_next_datablock(dir_node, total_block, dir_node->size, dir_node->size);

                write_data_to_block(new_block_index, (void *) newfile, sizeof(directory_entry));

                dir_node->size += sizeof(directory_entry);
                dir_node->last_block_index = new_block_index;

                if (total_block <= N_DBLOCKS) {
                    dir_node->dblocks[total_block - 1] = new_block_index;
                    // print_dir_block(node, node->dblocks[total_block-1]);
                } else {
                    printf("%s\n", "using iblocks of cur dir. TODO.");
                    free(new_inode);
                    return EXITFAILURE;
                }
                update_single_inode_ondisk(dir_node, dir_node->inode_index);

                //update inode for the new dir
                if (new_inode->inode_index != new_inode_index) {
                    printf("%s\n", "There is a problem.");
                    free(new_inode);
                    return EXITFAILURE;
                }
            } else {
                printf("%s\n", "not requesting new blocks");
                int block_offset = dir_node->size % BLOCKSIZE;
                memcpy(dir_data + block_offset, (void *) newfile, sizeof(directory_entry));
                write_data_to_block(dir_node->last_block_index, dir_data, BLOCKSIZE);
                //update parent dir inode
                printf("dir_node old size: %d\n", dir_node->size);
                dir_node->size += sizeof(directory_entry);
                printf("dir_node new size: %d\n", dir_node->size);
                printf("file_table dir_node size: %d\n", file_table[parent_fd]->file_inode->size);
                update_single_inode_ondisk(dir_node, dir_node->inode_index);
                if (new_inode->inode_index != new_inode_index) {
                    printf("%s\n", "There is a problem.");
                    free(new_inode);
                    return EXITFAILURE;
                }
            }

            // file_table[parent_fd]->byte_offset = 0;
            file_table_entry *file_entry = file_table[table_freehead];
            file_entry->free_file = FALSE;
            inode *file_inode = get_inode(newfile->inode_index);
            file_inode->type = REG;
            file_inode->parent_inode_index = dir_node->inode_index;
            file_inode->size = 0;
            file_inode->next_inode = -1;
            file_inode->dblocks[0] = current_mounted_disk->superblock1->free_block;
            // set_permissions(file_inode->permission, permissions);
            update_single_inode_ondisk(file_inode, file_inode->inode_index);
            // printf("NEW FILE INODE:\n");
            // print_inode(file_inode);
            free(file_entry->file_inode);
            file_entry->file_inode = file_inode;
            file_entry->byte_offset = 0;
            file_entry->access = access;
            // print_dir_block(dir_node, dir_node->last_block_index);
            // print_inode(dir_node);
            void *data = get_data_block(current_mounted_disk->superblock1->free_block);
            current_mounted_disk->superblock1->free_block = *(int *) data;
            update_superblock_ondisk(current_mounted_disk->superblock1);
            int fd = table_freehead;
            table_freehead = find_next_freehead();
            // free(dir_node);
            free(dir_data);
            free(dir);
            free(newfile);
            free(path);
            free(new_inode);
            free(data);
            // print_superblock(current_mounted_disk->superblock1);
            return fd;
        }
    }
}

void set_permissions(permission_value old_value, permission_value *new_value) {
    old_value.owner = new_value->owner;
    old_value.group = new_value->group;
    old_value.others = new_value->others;
}

int f_write(void* buffer, int size, int ntimes, int fd ) {
    //check if the file accociated with this fd has been open
    if (file_table[fd]->free_file == TRUE) {
        printf("%s\n", "The file must be open before write");
        return (EXITFAILURE);
    }
    if (file_table[fd]->access == READ) {
        printf("%s\n", "File is not writable.");
        return EXITFAILURE;
    }
    if (file_table[fd]->file_inode->type == REG) {
        printf("%s\n", "Writing to a regular file");
        superblock *sp = current_mounted_disk->superblock1;
        // printf("SUPERBLOCK VALUES\n");
        // print_superblock(sp);

        // printf("INODE VALUES:\n");
        // print_inode(file_table[fd]->file_inode);

        void *datatowrite = malloc(size * ntimes);
        memset(datatowrite, 0, size * ntimes);
        for (int j = 0; j < ntimes; j++) {
            memcpy(datatowrite + (j * size), buffer, size);
        }
        // ((char *) datatowrite)[size*ntimes] = "\0";
        // printf("DATA TO WRITE: %s\n", datatowrite);

        //this is the number of bytes left to write to the file...
        int lefttowrite = size * ntimes;

        if (file_table[fd]->access == APPEND || file_table[fd]->access == WRITE ||
            file_table[fd]->access == READANDWRITE) {
            //get the last data block of the file.
            void *last_data_block = malloc(BLOCKSIZE);
            memset(last_data_block, 0, BLOCKSIZE);

            int index = -1;
            // printf("FILE SIZE: %d, BLOCK INDEX %d\n", file_table[fd]->file_inode->size,  file_table[fd]->file_inode->size / BLOCKSIZE);
            void *copy = get_block_from_index(file_table[fd]->file_inode->size / BLOCKSIZE, file_table[fd]->file_inode,
                                              &index);

            // printf("Index of block IN FILEWRITE %d\n", index);
            // print_inode(file_table[fd]->file_inode);
            memcpy(last_data_block, copy, BLOCKSIZE);
            free(copy);
            // file_table[fd]->byte_offset = file_table[fd]->file_inode->size;
            int offset_into_last_block = 0;
            int old_offset = 0;
            int old_filesize = 0;
            if (file_table[fd]->access == APPEND) {
                offset_into_last_block = file_table[fd]->file_inode->size % BLOCKSIZE;
                old_offset = file_table[fd]->file_inode->size;
                printf("old file_size: %d\n", old_offset);
                old_filesize = old_offset;
            } else {
                offset_into_last_block = file_table[fd]->byte_offset % BLOCKSIZE;
                old_offset = file_table[fd]->byte_offset;
                old_filesize = file_table[fd]->file_inode->size;
            }
            int free_space = BLOCKSIZE - offset_into_last_block;
            if (free_space < size * ntimes && sp->free_block == -1 && free_space > 0) {
                printf("%s\n", "Not having enough space on the disk");
                free(datatowrite);
                free(last_data_block);
                return EXITFAILURE;
            }
            //TODO. complete the check of enough free space on disk
            int start_of_block_to_write = -1;
            int new_offset = old_offset + size * ntimes;
            // int file_offset = old_offset;
            int datatowrite_offset = 0;
            int total_block = 0;
            if (free_space == BLOCKSIZE) {
                if (file_table[fd]->byte_offset != 0) {
                    printf("%s\n", "dont need to fill the the last block");
                    // if (file_table[fd]->access != APPEND) {
                        int block_written = file_table[fd]->byte_offset / BLOCKSIZE;

                        total_block = block_written + 1; // the block we are writing to in the future
                        // printf("HELLO BLOCKS WRITTEN %d\n", total_block);
                    // }
                }
                // else {
                //     total_block += 1;
                // }
            } else {
                printf("%s\n", "do need to fill in the last block");
                void *data = malloc(BLOCKSIZE);
                memset(data, 0, BLOCKSIZE);
                //copy the data from the last block to data
                memcpy(data, last_data_block, offset_into_last_block);
                if ((size * ntimes) < free_space) {
                    memcpy(data + offset_into_last_block, datatowrite, size * ntimes);
                } else {
                    memcpy(data + offset_into_last_block, datatowrite, free_space);
                }
                if (file_table[fd]->access == APPEND) {
                    printf("%s\n", "appending to file");
                    // write_data_to_block(file_table[fd]->file_inode->last_block_index, data, sp->size);
                    write_data_to_block(index, data, sp->size);
                } else {
                    printf("%s\n", "writing to file");
                    total_block = file_table[fd]->byte_offset / BLOCKSIZE + 1;
                    start_of_block_to_write = find_next_datablock(file_table[fd]->file_inode, total_block, old_filesize,
                                                                  file_table[fd]->byte_offset);
                    write_data_to_block(start_of_block_to_write, data, sp->size);
                }
                if (free_space < (ntimes * size)) {
                    printf("updating size UPDATE\n");
                    file_table[fd]->byte_offset += free_space;
                    file_table[fd]->file_inode->size += free_space;
                    datatowrite_offset += free_space;
                } else {
                    printf("updating size UPDATE\n");
                    file_table[fd]->byte_offset += ntimes * size;
                    file_table[fd]->file_inode->size += ntimes * size;
                    datatowrite_offset += ntimes * size;
                }
                printf("INODE INDEX: %d\n", file_table[fd]->file_inode->inode_index);
                update_single_inode_ondisk(file_table[fd]->file_inode, file_table[fd]->file_inode->inode_index);
                //TODO: comment out at some point...
                inode *inode1 = get_inode(file_table[fd]->file_inode->inode_index);
                printf("INODE ON DISK GOTTEN AFTER WRITING!\n");
                // print_inode(inode1);
                printf(" BYTE OFFSET %d & FILE SIZE %d\n", file_table[fd]->byte_offset, file_table[fd]->file_inode->size);
                lefttowrite -= free_space;
                printf("lefttowrite: %d\n", lefttowrite);
                free(data);
                free(last_data_block);
                // file_offset += free_space;
                total_block += 1;
            }
            while (lefttowrite > 0) {
                start_of_block_to_write = find_next_datablock(file_table[fd]->file_inode, total_block, old_filesize,
                                                              file_table[fd]->byte_offset);
                printf("lefttowrite: %d\n", lefttowrite);
                int size_to_write = BLOCKSIZE;
                void *data = malloc(BLOCKSIZE);
                memset(data, 0, BLOCKSIZE);
                if (lefttowrite < BLOCKSIZE) {
                    size_to_write = lefttowrite;
                }
                memcpy(data, datatowrite + datatowrite_offset, size_to_write);
                write_data_to_block(start_of_block_to_write, data, size_to_write);
                lefttowrite -= size_to_write;
                // file_offset += size_to_write;
                free(data);
                file_table[fd]->byte_offset += size_to_write;
                if (file_table[fd]->access == APPEND) {
                    file_table[fd]->file_inode->size += size_to_write;
                } else {
                    if (file_table[fd]->file_inode->size < file_table[fd]->byte_offset) {
                        file_table[fd]->file_inode->size = file_table[fd]->byte_offset;
                    }
                }
                total_block += 1;
                file_table[fd]->file_inode->last_block_index = start_of_block_to_write;
                update_single_inode_ondisk(file_table[fd]->file_inode, file_table[fd]->file_inode->inode_index);
                printf("TOTAL_BLOCK: %d\n", total_block);
            }
            file_table[fd]->byte_offset = new_offset;
            printf("new_offset: %d\n", new_offset);
            free(datatowrite);
            printf("total_blocks: %d\n", total_block);

            inode *inode1 = get_inode(file_table[fd]->file_inode->inode_index);
            printf("INODE ON DISK GOTTEN AFTER WRITING!\n");
            // print_inode(inode1);
            return size * ntimes;
        }
    } else if (file_table[fd]->file_inode->type == DIR) {
        printf("%s\n", "Error. Attempting to write to a directory file!\n");
        //make_dir should do the same thing
    }
    return EXITSUCCESS;
}

boolean f_close(int file_descriptor) {
    if (file_descriptor >= FILETABLESIZE || file_descriptor < 0) {
        printf("I am sorry, but that is an invalid file descriptor.\n");
        return FALSE;
    } else {
        file_table[file_descriptor]->free_file = TRUE;
        // free(file_table[file_descriptor]->file_inode);
        table_freehead = find_next_freehead();
        return TRUE;
    }

    return FALSE;
}

boolean f_rewind(int file_descriptor) {
    if (file_descriptor >= FILETABLESIZE) {
        return FALSE;
    } else {
        file_table[file_descriptor]->byte_offset = 0;
        return TRUE;
    }
}

//TODO: finish this method and consider if offset can be negative??
boolean f_seek(int file_descriptor, int offset, int whence) {
    if (file_descriptor >= 0 && file_descriptor < FILETABLESIZE) {
        if (offset < 0) {
            printf("Failure in fseek - invalid offset value.\n");
            return FALSE;
        } else {
            inode *file_inode = file_table[file_descriptor]->file_inode;
            int file_size = file_inode->size;

            if (whence == SEEKSET) {
                if (offset <= file_size) {
                    file_table[file_descriptor]->byte_offset = offset;
                    return TRUE;
                } else {
                    printf("Failure in fseek - invalid offset value.\n");
                    return FALSE;
                }
            } else if (whence == SEEKCUR) {
                if (file_table[file_descriptor]->byte_offset + offset <= file_size) {
                    file_table[file_descriptor]->byte_offset += offset;
                    return TRUE;
                }
            } else if (whence == SEEKEND) {
                return TRUE;
            } else {
                printf("Failure in f_seek - invalid whence value.\n");
                return FALSE;
            }
        }
    } else {
        printf("Failure in f_seek - invalid file descriptor.\n");
        return FALSE;
    }
    return FALSE;
}

boolean f_stat(inode *inode1, stat *st) {
    st->size = inode1->size;
    st->uid = inode1->uid;
    st->gid = inode1->gid;
    st->ctime = inode1->ctime;
    st->mtime = inode1->mtime;
    st->atime = inode1->atime;
    st->type = inode1->type;
    st->permission.owner = inode1->permission.owner;
    st->permission.group = inode1->permission.group;
    st->permission.others = inode1->permission.others;
    st->inode_index = inode1->inode_index;

    return TRUE;
}

void print_inode (inode *entry) {
    printf("disk identifier: %d\n", entry->disk_identifier);
    printf("parent inode index: %d\n", entry->parent_inode_index);
    printf("next inode: %d\n", entry->next_inode);
    printf("file size: %d\n", entry->size);
    printf("file uid: %d\n", entry->uid);
    printf("file gid: %d\n", entry->gid);
    printf("file ctime: %d\n", entry->ctime);
    printf("file mtime: %d\n", entry->mtime);
    printf("file atime: %d\n", entry->atime);
    printf("file type: %d\n", entry->type);
    printf("file permissions owner: %d\n", entry->permission.owner);
    printf("file permissions group: %d\n", entry->permission.group);
    printf("file permissions others: %d\n", entry->permission.others);
    printf("inode index: %d\n", entry->inode_index);  //TODO: add printing out all file blocks as needed
    for (int i = 0; i < N_DBLOCKS; i++) {
        printf("%d th block with inode index %d\n", i, entry->dblocks[i]);
    }
    for (int j = 0; j < N_IBLOCKS; j++) {
        printf("%d th block with inode index %d\n", j, entry->iblocks[j]);
    }
    printf("i2block index: %d\n", entry->i2block);
    printf("i3block index: %d\n", entry->i3block);
    printf("last block index: %d\n", entry->last_block_index);
}

void print_dir_block(inode* node, int block_index) {
    void *data = get_data_block(block_index);
    // printf("how much needed to be print: %d\n", node->size);
    for (int i = 0; i < node->size; i += sizeof(directory_entry)) {
        printf("inode_index: %d, filename: %s\n", *(int *) (data + i), (char *) (data + i + sizeof(int)));
    }
    free(data);
}

void print_file_table() {
    printf("%s\n", "------------start printing file_table------");
    for (int i = 0; i < FILETABLESIZE; i++) {
        if (file_table[i] != NULL) {
            if (file_table[i]->free_file == TRUE) {
                printf("%d: %s\n", i, "empty");
            } else {
                inode *node = file_table[i]->file_inode;
                printf("%d: inode->index: %d \t byte_offset: %d\n", i, node->inode_index, file_table[i]->byte_offset);
            }
        }
    }
}

void print_table_entry (file_table_entry *entry) {
    printf("free file: %d\n", entry->free_file);
    print_inode(entry->file_inode);
    printf("byte offset: %d\n", entry->byte_offset);
    printf("access information %d\n", entry->access);
}

void print_superblock(superblock *superblock1) {
    printf("size: %d\n", superblock1->size);
    printf("inode offset: %d\n", superblock1->inode_offset);
    printf("data offset: %d\n", superblock1->data_offset);
    printf("free inode: %d\n", superblock1->free_inode);
    printf("free block: %d\n", superblock1->free_block);
    printf("root dir: %d\n", superblock1->root_dir);
}

directory_entry* f_opendir(char* filepath) {
    //parse the filepath
    char path[strlen(filepath) + 1];
    char copy[strlen(filepath) + 1];
    strcpy(path, filepath);
    strcpy(copy, filepath);
    char *s = "/'";
    char *token;
    //calculate the level of depth
    token = strtok(copy, s);
    int count = 0;
    while (token != NULL) {
        count++;
        token = strtok(NULL, s);
    }

    //create directory_entry for the root
    directory_entry *dir_entry = NULL;
    inode *root_node = get_inode(0);
    //add root directory to the open_file_table. Assume it is found.
    //if root already exists, should not add any more.
    if (file_table[0]->free_file == TRUE) {
        file_table_entry *root_table_entry = file_table[0];
        root_table_entry->free_file = FALSE;
        free(root_table_entry->file_inode);
        root_table_entry->file_inode = root_node;
        root_table_entry->byte_offset = 0;
        root_table_entry->access = READANDWRITE; //TODO: tell Rose about this...
        // memset(root_dir_entry, 0, sizeof(directory_entry));
        root_dir_entry->inode_index = 0;
        root_dir_entry->filename[0] = '/';
        root_dir_entry->filename[1] = 0;
        dir_entry = root_dir_entry;
    } else {
        printf("%s\n", "root already exists, in opendir-----2");
        // print_file_table();
        file_table[0]->byte_offset = 0;
        // memset(root_dir_entry, 0, sizeof(directory_entry));
        // root_dir_entry->inode_index = 0;
        // strncpy(root_dir_entry->filename, "/",1);
        // root_dir_entry->filename[0] = '/';
        // root_dir_entry->filename[1] = 0;
        dir_entry = root_dir_entry;
        printf("dir_entry info : %s\n", dir_entry->filename);
        printf("dir_entry info: %d\n", dir_entry->inode_index);
        free(root_node);
    }
    table_freehead = find_next_freehead();
    token = strtok(path, s);
    printf("token: %s\n", token);
    int i = 0;
    while (token != NULL && count >= 1) {
        //get the directory entry
        int found = FALSE;
        file_table[i]->byte_offset = 0;
        while (found == FALSE) {
            printf("%s , %d\n", token, i);
            dir_entry = f_readdir(i);
            if (dir_entry == NULL) {
                //reach the last byte in the file
                break;
            }
            char *name = dir_entry->filename;
            // printf("%s\n", name);
            if (strcmp(name, token) == 0) {
                // printf("%s is %s\n", token, "found" );
                file_table[i]->byte_offset = 0;
                found = TRUE;
                break;
            }
            if (found == FALSE) {
                free(dir_entry);
            }
        }
        if (i != 0) {
            // printf("what should be removed: %d\n", i);
            // print_file_table();
            //remove the ith index from the table. NEED CHECK. ROSE //TODO: ask her about this...
            file_table[i]->free_file = TRUE;
            table_freehead = i;
        } else {
            //root is always in the table. won't be removed.
            table_freehead = find_next_freehead();
            // i = table_freehead;
        }
        if (found == FALSE) {
            // printf("%s is not found\n", token);
            return NULL;
        }
        count--;
        //add dir_entry to the table
        //need to check if the dir_entry already exists
        inode *node = get_inode(dir_entry->inode_index);
        int fd = already_in_table(node);
        if (fd == -1) {
            file_table_entry *table_ent = file_table[table_freehead];
            table_ent->free_file = FALSE;
            table_ent->file_inode = node;
            table_ent->byte_offset = 0;
            table_ent->access = READANDWRITE; //TODO: tell Rose about this...
            i = table_freehead;
        } else {
            i = fd;
            free(node);
        }
        token = strtok(NULL, s);
    }
    table_freehead = find_next_freehead();
    printf("%s\n", "end of open_dir-----");
    return dir_entry;
}

int get_table_freehead() {
    return table_freehead;
}

directory_entry* f_mkdir(char* filepath) {
    //filepath need to be converted to absolute path beforehand
    char *newfolder = NULL;
    char *path = malloc(strlen(filepath));
    memset(path, 0, strlen(filepath));
    char path_copy[strlen(filepath) + 1];
    char copy[strlen(filepath) + 1];
    strcpy(path_copy, filepath);
    strcpy(copy, filepath);
    char *s = "/";
    //calculate the level of depth of dir
    char *token = strtok(copy, s);
    int count = 0;
    while (token != NULL) {
        count++;
        token = strtok(NULL, s);
    }
    // printf("count : %d\n", count);
    newfolder = strtok(path_copy, s);
    while (count > 1) {
        count--;
        path = strcat(path, "/");
        path = strcat(path, newfolder);
        newfolder = strtok(NULL, s);
    }
    // printf("path: %s\n", path);
    // printf("newfolder: %s\n", newfolder);
    directory_entry *dir = f_opendir(path);
    free(path);
    if (dir == NULL) {
        printf("cannot create directory. parent does not exists\n");
        free(dir);
        return NULL;
    } else {
        // printf("dir->filename: %s\n", dir->filename);
        // printf("dir->inode: %d\n", dir->inode_index);
        // printf("%s\n", "parent dir exists so here");
        directory_entry *newf = (directory_entry *) malloc(sizeof(directory_entry));
        memset(newf, 0, sizeof(directory_entry));
        // printf("%s\n", "+++++2");
        strcpy(newf->filename, newfolder);
        int new_inode_index = current_mounted_disk->superblock1->free_inode;
        if (new_inode_index == -1) {
            printf("%s\n", "not enough space to create new folder");
            free(newf);
            return NULL;
        }
        newf->inode_index = new_inode_index;
        directory_entry *parent = (directory_entry *) malloc(sizeof(directory_entry));
        memset(parent, 0, sizeof(directory_entry));
        // printf("%s\n", "++++++1");
        directory_entry *currentD = (directory_entry *) malloc(sizeof(directory_entry));
        memset(currentD, 0, sizeof(directory_entry));
        strcpy(currentD->filename, ".");
        strcpy(parent->filename, "..");
        //this node is the inode of parent dir
        int dir_fd = get_fd_from_inode_value(dir->inode_index);
        inode *node = file_table[dir_fd]->file_inode;
        printf("get inode accoring to new index: %d\n", new_inode_index);
        inode *new_inode = get_inode(new_inode_index);
        printf("result of the inode: %d\n", new_inode->inode_index);
        currentD->inode_index = new_inode_index;
        parent->inode_index = node->inode_index;
        current_mounted_disk->superblock1->free_inode = new_inode->next_inode;
        // printf("new_free inode: %d\n", new_inode->next_inode);
        void *dir_data = get_data_block(node->last_block_index);

        if (node->size == BLOCKSIZE * (node->size / BLOCKSIZE)) {
            //request new blocks to add the newfolder directory_entry
            // int new_block_index = request_new_block();
            // printf("%s\n", "need to request_new_block in mkdir");
            int total_block = node->size / BLOCKSIZE + 1;
            int new_block_index = find_next_datablock(node, total_block, node->size, node->size);
            void *content = malloc(BLOCKSIZE);
            memset(content, 0, BLOCKSIZE);
            memcpy(content, newf, sizeof(directory_entry));
            write_data_to_block(new_block_index, content, sizeof(directory_entry));
            //update inodes of parent dir
            //get inode_index
            node->size += sizeof(directory_entry);
            node->last_block_index = new_block_index;
            if (total_block <= N_DBLOCKS) {
                node->dblocks[total_block - 1] = new_block_index;
                // print_dir_block(node, node->dblocks[total_block-1]);
            } else {
                // printf("%s\n", "using iblocks of cur dir. TODO.");
                return NULL;
            }
            update_single_inode_ondisk(node, node->inode_index);
            //update inode for the new dir
            new_inode->size = sizeof(directory_entry) * 2;
            new_inode->type = DIR;
            new_inode->parent_inode_index = node->inode_index;
            if (new_inode->inode_index != new_inode_index) {
                printf("%s\n", "There is a problem.");
            }
            // new_inode->dblocks[0] = new_block_index;
            new_inode->dblocks[0] = current_mounted_disk->superblock1->free_block;
            void *data = get_data_block(current_mounted_disk->superblock1->free_block);
            current_mounted_disk->superblock1->free_block = *(int *) data;
            memcpy(data, currentD, sizeof(directory_entry));
            memcpy(data + sizeof(directory_entry), parent, sizeof(directory_entry));
            write_data_to_block(new_inode->dblocks[0], data, BLOCKSIZE);
            update_superblock_ondisk(current_mounted_disk->superblock1);
            update_single_inode_ondisk(new_inode, new_inode_index);
            free(data);
            free(content);
        } else {
            //append to the parent dir data block without padding
            // printf("%s\n", "appending to the data in mkdir");
            int block_offset = node->size % BLOCKSIZE;
            memcpy(dir_data + block_offset, (void *) newf, sizeof(directory_entry));
            // printf("added to this data block: %d\n", node->last_block_index);
            write_data_to_block(node->last_block_index, dir_data, BLOCKSIZE);
            //update parent dir inode
            // printf("%s\n", "-------1");
            node->size += sizeof(directory_entry);
            // print_dir_block(node, node->last_block_index);
            update_single_inode_ondisk(node, node->inode_index);
            // printf("%s\n", "------2");
            //update new folder inode
            new_inode->size = 2 * sizeof(directory_entry);
            new_inode->type = DIR;
            new_inode->parent_inode_index = node->inode_index;
            // printf("parent_inode_index should be: %d\n", node->inode_index);
            if (new_inode->inode_index != new_inode_index) {
                printf("new_inode: %d\n", new_inode->inode_index);
                printf("new index should be: %d\n", new_inode_index );
                printf("%s\n", "There is a problem.");
            }
            new_inode->dblocks[0] = current_mounted_disk->superblock1->free_block;
            new_inode->last_block_index = new_inode->dblocks[0];
            void *data = get_data_block(current_mounted_disk->superblock1->free_block);
            current_mounted_disk->superblock1->free_block = *(int *) data;
            // printf("%s\n", "------3");
            memcpy(data, currentD, sizeof(directory_entry));
            memcpy(data + sizeof(directory_entry), parent, sizeof(directory_entry));
            write_data_to_block(new_inode->dblocks[0], data, BLOCKSIZE);
            // print_dir_block(new_inode, new_inode->dblocks[0]);
            // printf("%s\n", "-----4");
            //update superblock
            current_mounted_disk->superblock1->free_inode = new_inode->next_inode;
            // printf("%s\n", "-----5");
            update_superblock_ondisk(current_mounted_disk->superblock1);
            update_single_inode_ondisk(new_inode, new_inode_index);
            // print_superblock(current_mounted_disk->superblock1);
            free(data);
        }
        free(currentD);
        free(parent);
        //free(node);
        free(new_inode);
        free(dir_data);
        // free(dir);
        return newf;
    }
}

//TODO: update the time with the last accessed time, here!
int f_read(void *buffer, int size, int n_times, int file_descriptor) {
    // printf("size: %d, n_times %d, fd %d\n", size, n_times, file_descriptor);
    // printf("offset %d\n", file_table[file_descriptor]->byte_offset);
    if (file_descriptor < 0 || file_descriptor >= FILETABLESIZE) {
        printf("I am sorry, but the file descriptor is invalid.\n");
        return ERROR;
    }

    inode *file_to_read = file_table[file_descriptor]->file_inode;
    long file_offset = file_table[file_descriptor]->byte_offset;
    int bytes_remaining_in_file = file_to_read->size - file_offset;
    int block_to_read = file_offset / BLOCKSIZE;
    int bytes_to_read;
    int bytes_remaining_in_block;
    int block_offset;
    char *block_to_read_from;
    int access = file_table[file_descriptor]->access;

    if (access == APPEND || access == WRITE) {
        printf("I am sorry, but you are attempting to read a file opened for appending or writing. This is an invalid action.\n");
        return ERROR;
    }
    if (size <= 0 || n_times <= 0 || size > file_to_read->size || size * n_times > file_to_read->size ||
        size > bytes_remaining_in_file || size * n_times > bytes_remaining_in_file) {
        printf("I am sorry, but you are attempting to read an invalid number of bytes from the file.\n");
        return ERROR;
    }

    int buffer_index = 0;
    int bytes_read = size * n_times;

    for (int i = 0; i < n_times; i++) {
        bytes_to_read = size;
        // printf("bytes to read %d\n", bytes_to_read);
        block_offset = file_offset % 512;
        while (bytes_to_read > 0) {
          //  printf("%d\n", size);
            //printf("bytes to read %d\n", bytes_to_read);
            int block_index = -1;
            block_to_read_from = get_block_from_index(block_to_read, file_to_read, &block_index);

            bytes_remaining_in_block = 512 - block_offset - 1;
            if (bytes_remaining_in_block >= size) {
                bytes_to_read = size;
                for (int j = 0; j < bytes_to_read; j++) {
                    ((char *) buffer)[buffer_index] = block_to_read_from[block_offset];
                    buffer_index++;
                    block_offset++;
                }
                size -= bytes_to_read;
                // free_data_block(block_to_read_from); //TODO: free this memory at some point!
            } else { //bytes_remaining_in_block < size
                bytes_to_read = bytes_remaining_in_block;
                for (int k = 0; k < bytes_to_read; k++) {
                    ((char *) buffer)[buffer_index] = block_to_read_from[block_offset];
                    buffer_index++;
                    block_offset++;
                }
                size -= bytes_to_read;
                block_to_read++;
                // free_data_block(block_to_read_from); //TODO: free this memory at some point...
            }
        }
    }

    // memcpy(buffer, buffer_to_return, n_times * size);
    // printf("FILE BUFFER %s\n", (char *) buffer);
    return bytes_read;
}


int find_next_datablock(inode* inode, int total_block, int old_fileoffest, int current_offset) {
    //total_block: including the one we are writing to
    //current_offset: not including the bytes we are writing
    printf("%s\n", "in find next data_block");
    int idtotal = N_IBLOCKS * BLOCKSIZE / sizeof(int);
    int i2total = BLOCKSIZE * BLOCKSIZE / sizeof(int) / sizeof(int);
    int i3total = i2total * BLOCKSIZE / sizeof(int);
    int start_of_block_to_write = 0;
    int location = -1;
    if (current_offset >= old_fileoffest) {
        start_of_block_to_write = request_new_block();
        printf("requsted new_block: %d\n", start_of_block_to_write);
        if (total_block <= N_DBLOCKS) {
            location = DBLOCK;
        } else if (total_block - N_DBLOCKS <= idtotal) {
            location = IDBLOCK;
            int index = (total_block - N_DBLOCKS) / (BLOCKSIZE / sizeof(int));
            if (index * (BLOCKSIZE / sizeof(int)) == total_block - N_DBLOCKS) {
                inode->iblocks[index] = start_of_block_to_write;
                update_single_inode_ondisk(inode, inode->inode_index);
            }
        } else if (total_block - N_DBLOCKS - idtotal <= i2total) {
            location = I2BLOCK;
            return EXITFAILURE;
        } else if (total_block - N_DBLOCKS - idtotal - i2total <= i3total) {
            location = I3BLOCK;
            return EXIT_FAILURE;
        }
        update_inodes_datablocks(location, total_block, inode, start_of_block_to_write);
        return start_of_block_to_write;
    }
    if (total_block <= N_DBLOCKS) {
        start_of_block_to_write = inode->dblocks[total_block - 1];
    } else if (total_block - N_DBLOCKS <= idtotal) {
        int id_index = (total_block - N_DBLOCKS) / (BLOCKSIZE / sizeof(int));
        int data_offset = (total_block - N_DBLOCKS) % (BLOCKSIZE / sizeof(int)) - 1;
        int data_index = inode->iblocks[id_index];
        void *data = get_data_block(data_index);
        start_of_block_to_write = *(int *) (data + data_offset * sizeof(int));
        free(data);
    } else if (total_block - N_DBLOCKS - idtotal <= i2total) {
        int rest_block = total_block - N_DBLOCKS - idtotal;
        int data_offset1 = rest_block / (BLOCKSIZE / sizeof(int));
        void *data1 = get_data_block(inode->i2block);
        int data2_index = *(int *) (data1 + data_offset1 * sizeof(int));
        void *data2 = get_data_block(data2_index);
        int data_offset2 = (rest_block - (BLOCKSIZE / sizeof(int)) * data_offset1) % (BLOCKSIZE / sizeof(int));
        start_of_block_to_write = data_offset2;
        free(data2);
        free(data1);
    } else if (total_block - N_DBLOCKS - idtotal - i2total <= i3total) {
        int rest_block = total_block - N_DBLOCKS - idtotal - i2total;
        void *data1 = get_data_block(inode->i3block);
        int data_offset1 = rest_block / (BLOCKSIZE / sizeof(int)) / (BLOCKSIZE / sizeof(int));
        int data2_index = *(int *) (data1 + (data_offset1) * sizeof(int));
        void *data2 = get_data_block(data2_index);
        rest_block = rest_block - data_offset1 * BLOCKSIZE / sizeof(int) * BLOCKSIZE / sizeof(int);
        int data_offset2 = rest_block / (BLOCKSIZE / sizeof(int));
        int data3_index = *(int *) (data2 + (data_offset2) * sizeof(int));
        void *data3 = get_data_block(data3_index);
        int data_offset3 = *(int *) (data3 + (data3_index) * sizeof(int));
        free(data3);
        free(data2);
        free(data1);
        start_of_block_to_write = data_offset3;
    }
    return start_of_block_to_write;
}

int update_inodes_datablocks(int inode_loc, int total_block, inode* node, int data_index) {
    // printf("%s\n", "update inode data blocks");
    int num_entry_perblock = BLOCKSIZE / sizeof(int);
    int idtotal = N_IBLOCKS * num_entry_perblock;
    int i2total = num_entry_perblock * num_entry_perblock;
    // int i3total = i2total * num_entry_perblock;
    if (inode_loc == DBLOCK) {
        // printf("%s\n", "update dblocks");
        // printf("total_block: %d\n", total_block);
        node->dblocks[total_block-1] = data_index;
        update_single_inode_ondisk(node, node->inode_index);
    } else if (inode_loc == IDBLOCK) {
        // printf("%s\n", "************update iblocks***********");
        int index = (total_block - N_DBLOCKS) / num_entry_perblock;
        // if(index >= N_IBLOCKS){
        //   printf("index: %d\n", index);
        //   printf("N_IBLOCKS: %d\n", N_IBLOCKS);
        //   print_inode(node);
        //   printf("%s\n", "inode_loc is not calculated correctly");
        //   exit(EXITFAILURE);
        // }
        void *data1 = get_data_block(node->iblocks[index + 1]);
        int index2 = (total_block - N_DBLOCKS) % num_entry_perblock;
        memcpy(data1 + index2 * sizeof(int), &data_index, sizeof(int));
        write_data_to_block(node->iblocks[index + 1], data1, BLOCKSIZE);
        free(data1);
    } else if (inode_loc == I2BLOCK) {
        int index = (total_block - N_DBLOCKS - idtotal) / num_entry_perblock;
        void *data1 = get_data_block(node->i2block);
        int index2 = *(int *) (data1 + index * sizeof(int));
        void *data2 = get_data_block(index2);
        int offset = (total_block - N_DBLOCKS - idtotal) % num_entry_perblock;
        memcpy(data2 + offset * sizeof(int), &data_index, sizeof(int));
        write_data_to_block(index2, data2, BLOCKSIZE);
        free(data2);
        free(data1);
    } else if (inode_loc == I3BLOCK) {
        int index = (total_block - N_DBLOCKS - idtotal - i2total) / num_entry_perblock / num_entry_perblock;
        void *data1 = get_data_block(node->i3block);
        int index2 = *(int *) (data1 + index * sizeof(int));
        void *data2 = get_data_block(index2);
        int index3 = (total_block - N_DBLOCKS - idtotal - i2total) / num_entry_perblock;
        void *data3 = get_data_block(index3);
        int offset = (total_block - N_DBLOCKS - idtotal - i2total) % num_entry_perblock;
        memcpy(data3 + offset * sizeof(int), &data_index, sizeof(int));
        write_data_to_block(index3, data3, BLOCKSIZE);
        free(data3);
        free(data2);
        free(data1);
    }
    return EXITSUCCESS;
}


//TODO: pont define and figure out if you're requesting a block that doesn't exist
//TODO: check that this isn't just off by one
void *get_block_from_index(int block_index, inode *file_inode, int *data_region_index) { //block index is block overall to get...more computations must be done to know which block precisely to obtain
    //get direct block
    // printf("block index to get %d\n", block_index);
    // printf("actual value in block array %d\n", file_inode->dblocks[block_index]);
    // print_inode(file_inode);
    block *block_to_return = NULL;
    if (block_index >= 0 && block_index <= 9) {
        block_to_return = get_data_block(file_inode->dblocks[block_index]);
        *data_region_index = file_inode->dblocks[block_index];
    } else if (block_index >= 10 && block_index <= 521) {
        block_index -= 10; //adjust to array here
        int indirect_array_index = block_index / 128;
        int array_index = block_index % 128;
        int *indirect_block = get_data_block(file_inode->iblocks[indirect_array_index]);
        block_to_return = get_data_block(indirect_block[array_index]);
        *data_region_index = indirect_block[array_index];
        free_data_block(indirect_block);
    } else if (block_index >= 522 && block_index <= 16905) {
        block_index -= 522; //adjust to array here
        int indirect_array_index = block_index / 128;
        int array_index = block_index % 128;
        int *double_indirect_block = get_data_block(file_inode->i2block);
        int *indirect_block = get_data_block(double_indirect_block[indirect_array_index]);
        block_to_return = get_data_block(indirect_block[array_index]);
        *data_region_index = indirect_block[array_index];
        free_data_block(double_indirect_block);
        free_data_block(indirect_block);
    } else if (block_index >= 16906 && block_index <= 2114057) {
        block_index -= 16906;
        int double_indirect_array_index = block_index / 128 / 128;
        int indirect_array_index = block_index / 128;
        int array_index = block_index % 128;
        int *triple_indirect_block = get_data_block(file_inode->i3block);
        int *double_indirect_block = get_data_block(triple_indirect_block[double_indirect_array_index]);
        int *indirect_block = get_data_block(double_indirect_block[indirect_array_index]);
        block_to_return = get_data_block(indirect_block[array_index]);
        *data_region_index = indirect_block[array_index];
        free_data_block(triple_indirect_block);
        free_data_block(double_indirect_block);
        free_data_block(indirect_block);
    } else {
        printf("Index out of bounds for get block from index.\n");
    }
    return block_to_return;
}

int get_size_directory_entry(directory_entry* entry) {
    int index = entry->inode_index;
    inode *dirnode = get_inode(index);
    int size = dirnode->size;
    free(dirnode);
    return size;
}

directory_entry* f_rmdir(char* filepath) {
    directory_entry *start_ent = f_opendir(filepath);
    if (start_ent == NULL) {
        printf("%s does not exit so cannot remove in rmdir\n", filepath);
        return NULL;
    }
    inode *start_node = get_inode(start_ent->inode_index);
    printf("start ent name: %s\n", start_ent->filename);
    f_rmdir_helper(filepath, start_node);
    return start_ent;
}

int get_size_directory_entry_block(directory_entry* entry) {
    int index = entry->inode_index;
    inode *dirnode = get_inode(index);
    int size = dirnode->size;
    free(dirnode);
    return size;
}

directory_entry* get_first_direntry(inode* node) {
    printf("%s\n", "in get fiest dirent");
    directory_entry *result = malloc(sizeof(directory_entry));
    memset(result, 0, sizeof(directory_entry));
    void *data = get_data_block(node->dblocks[0]);
    int index = *(int *) (data + 2 * sizeof(directory_entry));
    char *name = (char *) (data + 2 * sizeof(directory_entry) + sizeof(int));
    result->inode_index = index;
    printf("name: %s\n", name);
    strcpy(result->filename, name);
    free(data);
    return result;
}

void f_rmdir_helper(char* filepath, inode* node) {
    if (node->type == REG) {
        printf("%s\n", "should not be here");
        f_remove(filepath);
        free(filepath);
        free(node);
    } else {
        if (node->size == sizeof(directory_entry) * 2) {
            printf("%s\n", "empty dir ready to remove in helper");
            //the directroy is empty
            //remove the directory
            f_remove(filepath);
            free(filepath);
            free(node);
        } else {
            printf("%s\n","not ready to remove in helper" );
            directory_entry *entry = get_first_direntry(node);
            free(node);
            printf("should be same as name: %s\n",entry->filename );
            inode *new_node = get_inode(entry->inode_index);
            int new_length = strlen(filepath) + strlen(entry->filename) + 2;
            char *new_path = malloc(new_length);
            memset(new_path, 0, new_length);
            new_path = strncat(new_path, filepath, strlen(filepath));
            new_path = strncat(new_path, "/", 1);
            new_path = strcat(new_path, entry->filename);
            free(entry);
            printf("newpath in helper: %s\n", new_path);
            f_rmdir_helper(new_path, new_node);
        }
    }
}

boolean f_remove(char *filepath) {
    //TODO: make a method for the following (ask Rose)
    //get the filename and the path seperately
    printf("%s\n", "in f_remove------------------------");
    char *filename = NULL;
    char *path = malloc(strlen(filepath) + 1);
    memset(path, 0, strlen(filepath) + 1);
    char path_copy[strlen(filepath) + 1];
    char copy[strlen(filepath) + 1];
    strcpy(path_copy, filepath);
    strcpy(copy, filepath);
    char *s = "/'";
    //calculate the level of depth of dir
    char *token = strtok(copy, s);
    int count = 0;
    while (token != NULL) {
        count++;
        token = strtok(NULL, s);
    }
    // printf("count : %d\n", count);
    filename = strtok(path_copy, s);
    while (count > 1) {
        count--;
        path = strcat(path, "/");
        path = strcat(path, filename);
        filename = strtok(NULL, s);
    }
    //TODO: need to check if the file is already in the file_table. Any sart way of doing that?
    directory_entry *dir = f_opendir(path);
    if (dir == NULL) {
        printf("%s\n", "directory does not exist");
        free(path);
        return FALSE;
    } else {
        printf("dir->name: %s\n", dir->filename);

        int index = -1;
        inode *directory_inode = get_inode_from_file_table_from_directory_entry(dir, &index);
        superblock *superblock1 = current_mounted_disk->superblock1;
        print_dir_block(directory_inode, directory_inode->last_block_index);
        if (directory_inode == NULL) {
            return FALSE;
        } else {
            //now, I have the inode for the file...and the disk's superblock! :)
            //TODO: Remove the file from its directory
            directory_entry *current_entry = f_readdir(index);
            directory_entry *directory_to_replace = NULL;
            // int byte_offset_of_directory_to_replace = 0;
            while (current_entry != NULL){
              if(strcmp(current_entry->filename, filename) != 0) {
                current_entry = f_readdir(index);
                // byte_offset_of_directory_to_replace += sizeof(directory_entry);
              }else{
                break;
              }
            }


            if (current_entry == NULL) {
                printf("%s\n", "no such file. retry");
                return FALSE; //wasn't able to find the entry in the directory
            } else {
                printf("entry->filename: %s\n", current_entry->filename);
                printf("entry->index: %d\n", current_entry->inode_index);
                directory_to_replace = current_entry;
            }
            // print_file_table();
            // printf("somehow changed? : %d\n", directory_to_replace->inode_index);
            inode *file_inode = get_inode(directory_to_replace->inode_index);
            // print_inode(file_inode);
            // printf("let's see : %d\n", file_inode->inode_index);
            if (already_in_table(file_inode) != -1) {
                printf("I am sorry, you are attempting to remove a file that is open. Please close and try again!\n");
                return FALSE;
            }

            //get the last directory in the directory file and input it's information into this place
            int directory_file_size = directory_inode->size;
            int last_directory_byte_index = directory_file_size - sizeof(directory_entry);
            //check if the directory entry is the only one in the data block... (if so, then we need to reclaim the data block...)
            printf("last_directory_byte_index: %d\n", last_directory_byte_index);
            int block_to_fetch = last_directory_byte_index / BLOCKSIZE;

            directory_entry *directory_to_move = malloc(sizeof(directory_entry));
            memset(directory_to_move, 0, sizeof(directory_entry));
            int final_block_index = -1;
            void *data_block_containing_final_directory_entry = get_block_from_index(block_to_fetch, directory_inode,
                                                                                     &final_block_index);
            fseek(current_mounted_disk->disk_image_ptr, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK +
                                                        current_mounted_disk->superblock1->data_offset *
                                                        current_mounted_disk->superblock1->size +
                                                        final_block_index * BLOCKSIZE +
                                                        last_directory_byte_index % BLOCKSIZE, SEEK_SET);
            fread(directory_to_move, sizeof(directory_entry), 1, current_mounted_disk->disk_image_ptr);
            printf("final block index: %d\n", final_block_index);
            //need to write directory_to_replace back to the disk, now, since it has been deleted...
            directory_to_replace->inode_index = directory_to_move->inode_index;
            printf("directory_to_move inode:%d\n",directory_to_move->inode_index );
            strcpy(directory_to_replace->filename, directory_to_move->filename);
            printf("dir to replace: %s\n", directory_to_replace->filename);
            // fseek(); //TODO: replace with our fseek when needed
            file_table[index]->byte_offset -= sizeof(directory_entry); //pointer in file is now at the correct location to fwrite...
            int current_block_index = -1;
            int second_block_to_fetch = file_table[index]->byte_offset / BLOCKSIZE;
            void *data_block_containing_directory_to_replace = get_block_from_index(second_block_to_fetch,
                                                                                    directory_inode,
                                                                                    &current_block_index);
            fseek(current_mounted_disk->disk_image_ptr, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK +
                                                        current_mounted_disk->superblock1->data_offset *
                                                        current_mounted_disk->superblock1->size +
                                                        current_block_index * BLOCKSIZE +
                                                        file_table[index]->byte_offset % BLOCKSIZE, SEEK_SET);
            fwrite(directory_to_replace, sizeof(directory_entry), 1, current_mounted_disk->disk_image_ptr);

            if (block_to_fetch * BLOCKSIZE == last_directory_byte_index) {
                //this means that the directory entry to move is at the start of a directory file block...
                //TODO: update last block index here!!
                //TODO: decrease directory file size and free the final data block in the directory by adding it to the free list in the superblock...
                // printf("block index to add to the free list %d\n", final_block_index);
                int *data_block = data_block_containing_final_directory_entry;
                data_block[0] = superblock1->free_block;
                superblock1->free_block = final_block_index;

                //write the data block to the disk and update the superblock
                fseek(current_mounted_disk->disk_image_ptr, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK +
                                                            current_mounted_disk->superblock1->data_offset *
                                                            current_mounted_disk->superblock1->size +
                                                            final_block_index * BLOCKSIZE, SEEK_SET);
                fwrite((void *) data_block_containing_final_directory_entry, BLOCKSIZE, 1,
                       current_mounted_disk->disk_image_ptr);
            } else {
                //this means that the directory entry to move is in the middle of a directory file block and so no reclaming is needed
            }

            free_data_block(data_block_containing_final_directory_entry);
            free_data_block(data_block_containing_directory_to_replace);

            directory_inode->size -= sizeof(directory_entry);
            printf("directory_inode size: %d\n", directory_inode->size);
            printf("directory_inode index(0): %d\n",directory_inode->inode_index );
            fseek(current_mounted_disk->disk_image_ptr,
                  SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + superblock1->inode_offset * BLOCKSIZE +
                  directory_inode->inode_index * sizeof(inode), SEEK_SET);
            fwrite(directory_inode, sizeof(inode), 1, current_mounted_disk->disk_image_ptr);

            //TODO: Release the i-node to the pool of free inodes and re-write the value to the disk...
            //TODO: talk to Rose and see what else needs to be done here..
            int new_head = file_inode->inode_index;
            // file_inode->next_inode = superblock1->free_inode;
            printf("new_head: %d\n", new_head);
            int old_head = superblock1->free_inode;
            //read in the current_head inode and write the new and correct value to disk...
            superblock1->free_inode = new_head;
            file_inode->next_inode = old_head;
            printf("old_head: %d\n",old_head );
            fseek(current_mounted_disk->disk_image_ptr,
                  SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + superblock1->inode_offset * BLOCKSIZE + new_head * sizeof(inode),
                  SEEK_SET);
            fwrite(file_inode, sizeof(inode), 1, current_mounted_disk->disk_image_ptr);

            //TODO: return the disk blocks to the pool of free disk blocks
            int file_size = file_inode->size;
            int file_blocks = ceilf((float) file_size / (float) BLOCKSIZE);
            void *block_to_replace;
            int block_value;

            for (int i = 0; i < file_blocks; i++) {
                block_to_replace = get_block_from_index(i, file_inode, &block_value);
                ((int *) block_to_replace)[0] = superblock1->free_block;
                superblock1->free_block = block_value;

                //write the data block to the disk and update the superblock
                fseek(current_mounted_disk->disk_image_ptr, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK +
                                                            current_mounted_disk->superblock1->data_offset *
                                                            current_mounted_disk->superblock1->size +
                                                            block_value * BLOCKSIZE, SEEK_SET);
                fwrite((void *) data_block_containing_final_directory_entry, BLOCKSIZE, 1,
                       current_mounted_disk->disk_image_ptr);
                free_data_block(block_to_replace);
            }
            //write the superblock to the disk...
            fseek(current_mounted_disk->disk_image_ptr, SIZEOFBOOTBLOCK, SEEK_SET );
              // + SIZEOFSUPERBLOCK + current_mounted_disk->superblock1->inode_offset *
                                                        // current_mounted_disk->superblock1->size +
                                                        // new_head * sizeof(inode), SEEK_SET);
            fwrite(superblock1, SIZEOFSUPERBLOCK, 1, current_mounted_disk->disk_image_ptr);

            //TODO: close the directory
            // f_closedir(dir);
            printf("%s\n", "endof f_remove");
            // print_superblock(superblock1);
            // inode* node = get_inode(new_head);
            // printf("check if inode correct: %d\n", node->inode_index);
            // print_file_table();
            return TRUE;
        }
    }
}

boolean f_closedir(directory_entry *entry) {
    if (entry == NULL) {
        return FALSE;
    } else {
        int inode_index = entry->inode_index;
        free(entry);
        int fd = -1;
        for (int i = 0; i < FILETABLESIZE; i++) {
            if (file_table[i]->file_inode->inode_index == inode_index) {
                //found the one you're searching for
                fd = i;
                break;
            }
        }

        if (fd == -1) {
            return FALSE;
        } else {
            return f_close(fd);
        }
    }
}

inode *get_inode_from_file_table_from_directory_entry(directory_entry *entry, int *table_index) {
    if (entry == NULL) {
        return NULL;
    } else {
        int inode_index = entry->inode_index;
        inode *directory_inode = NULL;
        for (int i = 0; i < FILETABLESIZE; i++) {
            if (!file_table[i]->free_file && file_table[i]->file_inode->inode_index == inode_index) {
                directory_inode = file_table[i]->file_inode;
                *table_index = i;
            }
        }
        return directory_inode;
    }
    return NULL;
}

directory_entry* f_readdir(int index_into_file_table) {
    // printf("INDEX INTO FILE TABLE %d\n", index_into_file_table);
    if (index_into_file_table < 0 || index_into_file_table >= FILETABLESIZE) {
        printf("Index into the file table is invalid.\n");
        return NULL;
    }

    //fetch the inode of the directory to be read (assume the directory file is already open in the file table and that you are given the index into the file table)
    long offset_into_file = file_table[index_into_file_table]->byte_offset;
//    printf("in readir offset_into_file: %d\n", offset_into_file);
    inode *current_directory = file_table[index_into_file_table]->file_inode;
    if (current_directory->size - offset_into_file < sizeof(directory_entry)) {
        // printf("file table byte offset: %ld\n", offset_into_file);
        // printf("index_file_table: %d\n", index_into_file_table);
        // printf("Error! Attempting to read past the end of the directory file.\n");
        return NULL;
    }

    superblock *superblockPtr = current_mounted_disk->superblock1;
    // long directory_index_in_block = offset_into_file / sizeof(directory_entry);
    long block = ((float) offset_into_file /
                  (float) superblockPtr->size);
//    printf("%s\n", "-------");
//    printf("block: %d\n", block);
    directory_entry *next_directory = malloc(sizeof(directory_entry));
    memset(next_directory, 0, sizeof(directory_entry));
    long offset_in_block = offset_into_file - (superblockPtr->size * block);
    long num_indirect = superblockPtr->size / sizeof(int);
    // long num_directories = superblockPtr->size / sizeof(directory_entry);
    if (offset_into_file <= current_directory->size) {
        direct_copy(next_directory, current_directory, current_directory->dblocks[block], offset_in_block);
    } else if (offset_into_file > DBLOCKS && offset_into_file <= IBLOCKS) {
        long adjusted_block = block - N_DBLOCKS; //index into indirect block range
        long index_indirect = adjusted_block / num_indirect;
        long index = adjusted_block % num_indirect;
        indirect_copy(next_directory, current_directory, index, current_directory->iblocks[index_indirect],
                      offset_in_block);
    } else if (offset_into_file > IBLOCKS && offset_into_file <= I2BLOCKS) {
        long adjusted_block = block - N_DBLOCKS - N_IBLOCKS * num_indirect; //index into indirect block range
        long index_indirect = adjusted_block / num_indirect;
        long index = adjusted_block % num_indirect;

        int *indirect_block_array = get_data_block(current_directory->i2block);
        int block_to_fetch = *((int *) (indirect_block_array + index_indirect * sizeof(int)));
        free_data_block(indirect_block_array);

        indirect_copy(next_directory, current_directory, index, block_to_fetch, offset_in_block);
    } else if (offset_into_file > I2BLOCKS && offset_into_file <= I3BLOCKS) {
        long adjusted_block = block - N_DBLOCKS - N_IBLOCKS * num_indirect -
                              num_indirect * num_indirect; //index into double indirect block range
        long double_index_indirect = adjusted_block / num_indirect / num_indirect;
        int *doubly_indirect_block_array = get_data_block(current_directory->i3block);
        int indirect_block_to_fetch = *((int *) (doubly_indirect_block_array + double_index_indirect * sizeof(int)));
        free_data_block(doubly_indirect_block_array);

        long index_indirect = adjusted_block / num_indirect;
        long index = adjusted_block % num_indirect;
        int *indirect_block_array = get_data_block(indirect_block_to_fetch);
        int block_to_fetch = *((int *) (indirect_block_array + index_indirect * sizeof(int)));
        free_data_block(indirect_block_array);

        indirect_copy(next_directory, current_directory, index, block_to_fetch, offset_in_block);
    } else {
        free(next_directory);
        printf("File Size Too Large To Handle.\n");
        return NULL;
    }

    //increment offset into the file
    file_table[index_into_file_table]->byte_offset += sizeof(directory_entry);
    // printf("in readdir: %s\n", next_directory->filename);
    return next_directory;
}

void indirect_copy(directory_entry *entry, inode *current_directory, int index, long indirect_block_to_fetch, long offset_in_block) {
    void *indirect_data_block = get_data_block(indirect_block_to_fetch);
    int block_to_fetch = *((int *) (indirect_data_block + index * sizeof(int)));
    free_data_block(indirect_data_block);
    direct_copy(entry, current_directory, block_to_fetch, offset_in_block);
}

void direct_copy(directory_entry *entry, inode *current_directory, long block_to_fetch, long offset_in_block) {
//    printf("block_to_fetch: %d\n", block_to_fetch);
    void *data_block = get_data_block(block_to_fetch);
    // printf("%d\n", *(int *) data_block);
    memcpy(entry, data_block + offset_in_block, sizeof(directory_entry));
    free_data_block(data_block);
}

//TODO: add error when you're trying to read a data block that doesn't exist on the disk (a.k.a past the end of the disk...)
void *get_data_block(int index) {
    // printf("INDEX VALUE %d\n", index);
    void *data_block = malloc(current_mounted_disk->superblock1->size);
    memset(data_block, 0, current_mounted_disk->superblock1->size);
    // FILE *current_disk = current_mounted_disk->disk_image_ptr;
    fseek(current_mounted_disk->disk_image_ptr, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK +
                                                current_mounted_disk->superblock1->data_offset * BLOCKSIZE +
                                                index * BLOCKSIZE,
          SEEK_SET);
    fread(data_block, current_mounted_disk->superblock1->size, 1, current_mounted_disk->disk_image_ptr);
    // printf("Data block values: %s\n", (char *) data_block);
    // printf("data block index: %d\n", *(int*)data_block);
    return data_block;
}

int request_new_block() {
    superblock *sp = current_mounted_disk->superblock1;
    int free_block = sp->free_block;
    void *prevfree_block_on_disk = get_data_block(free_block);
    // printf("%s\n", "request_new_block +++++");
    int next_free = ((block *) prevfree_block_on_disk)->next_free_block;
    sp->free_block = next_free;
    update_superblock_ondisk(sp);
    // printf("%s\n", "--------in req new block-----");
    // print_superblock(sp);
    // printf("%s\n", "----------end----------");
    free(prevfree_block_on_disk);
    return free_block;
}

int update_superblock_ondisk(superblock* new_superblock) {
    FILE *current_disk = current_mounted_disk->disk_image_ptr;
    fseek(current_disk, SIZEOFBOOTBLOCK, SEEK_SET);
    // fwrite(new_superblock, SIZEOFSUPERBLOCK, 1, current_disk);
    fwrite(new_superblock, sizeof(superblock), 1, current_disk);
    int padding_size = SIZEOFSUPERBLOCK - sizeof(superblock);
    int padding = 0;
    fwrite(&padding, 1, padding_size, current_disk);
    return SIZEOFSUPERBLOCK;
}

int update_single_inode_ondisk(inode* new_inode, int new_inode_index) {
    // printf("new inode index %d\n", new_inode_index);
    // print_inode(new_inode);
    FILE *current_disk = current_mounted_disk->disk_image_ptr;
    superblock *sp = current_mounted_disk->superblock1;
    int total_inode_num = (sp->data_offset - sp->inode_offset) * sp->size / sizeof(inode);
    if (new_inode_index > total_inode_num) {
        printf("%s\n", "NOT ENOUGH SPACE FOR INODES");
        return EXITFAILURE;
    }
    // fseek(current_disk, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + sp->inode_offset * sp->size +
    // (new_inode->inode_index) * sizeof(inode), SEEK_SET);
    fseek(current_disk, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + sp->inode_offset * BLOCKSIZE +
                        new_inode_index * sizeof(inode), SEEK_SET);
    fwrite(new_inode, sizeof(inode), 1, current_disk);
    return sizeof(new_inode);
}

//intended to write to data region
int write_data_to_block(int block_index, void* content, int size) {
    superblock *sp = current_mounted_disk->superblock1;
    FILE *current_disk = current_mounted_disk->disk_image_ptr;
    if (size > sp->size) {
        printf("%s\n", "Writing too much into one block.write_data_to_block()");
        return EXITFAILURE;
    }
    fseek(current_disk, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + sp->size * (sp->data_offset + block_index), SEEK_SET);
    // if(fwrite(content, size, 1, current_disk) == size){
    //     return size;
    // }
    fwrite(content, size, 1, current_disk);
    return EXITSUCCESS;
}

int already_in_table(inode* node) {
    int index2 = node->inode_index;
    for (int i = 0; i < FILETABLESIZE; i++) {
        if (file_table[i] != NULL) {
            if (file_table[i]->free_file == FALSE) {
                int index1 = file_table[i]->file_inode->inode_index;
                if (index1 == index2) {
                    return i;
                }
            }
        }
    }
    return -1;
}


int find_next_freehead() {
    for (int i = 0; i < FILETABLESIZE; i++) {
        if (file_table[i]->free_file == TRUE) {
            return i;
        }
    }
    return EXITFAILURE;
}

void free_data_block(void *block_to_free) {
    free(block_to_free);
}

inode* get_inode(int index) {
    inode *node = malloc(sizeof(inode));
    memset(node, 0, sizeof(inode));
    FILE *current_disk = current_mounted_disk->disk_image_ptr;
    fseek(current_disk, SIZEOFBOOTBLOCK + SIZEOFSUPERBLOCK + index * sizeof(inode), SEEK_SET);
    fread(node, sizeof(inode), 1, current_disk);
    return node;
}

int addto_file_table(inode* node, int access) {
    // printf("%s\n", "in addto_file_table");
    int fd = already_in_table(node);
    if (fd != -1) {
        // printf("%s\n", "already_in_table. so not adding to file table");
        free(node);
        return fd;
    }
    fd = find_next_freehead();
    if (fd == -1) {
        // printf("%s\n", "No more space in file table. Sorry");
        return fd;
    }
    file_table[fd]->free_file = FALSE;
    free(file_table[fd]->file_inode);
    file_table[fd]->file_inode = node;
    file_table[fd]->byte_offset = 0;
    if (node->type == DIR) {
        file_table[fd]->access = APPEND;
    } else {
        file_table[fd]->access = access;
    }
    // print_file_table();
    return fd;
}


int remove_from_file_table(inode* node) {
    // printf("%s\n", "in remove_from_file_table");
    int fd = already_in_table(node);
    if (fd == -1) {
        printf("%s\n", "cannot remove. no such file in file_table");
        return fd;
    }
    file_table[fd]->free_file = TRUE;
    table_freehead = fd;
    return fd;
}
