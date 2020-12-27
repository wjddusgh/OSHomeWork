//
// Simple FIle System
// Student Name :   정연호
// Student Number : B511180
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

static struct sfs_super spb;	// superblock
static struct sfs_dir sd_cwd = { SFS_NOINO }; // current working directory

void error_message(const char *message, const char *path, int error_code) {
	switch (error_code) {
	case -1:
		printf("%s: %s: No such file or directory\n",message, path); return;
	case -2:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -3:
		printf("%s: %s: Directory full\n",message, path); return;
	case -4:
		printf("%s: %s: No block available\n",message, path); return;
	case -5:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -6:
		printf("%s: %s: Already exists\n",message, path); return;
	case -7:
		printf("%s: %s: Directory not empty\n",message, path); return;
	case -8:
		printf("%s: %s: Invalid argument\n",message, path); return;
	case -9:
		printf("%s: %s: Is a directory\n",message, path); return;
	case -10:
		printf("%s: %s: Is not a file\n",message, path); return;
	case -11:
		printf("%s: can't open %s input file\n", message, path); return;
	case -12:
		printf("%s: input file size exceeds the max file size\n", message); return;
	default:
		printf("unknown error code\n");
		return;
	}
}


void sfs_mount(const char* path)
{
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read( &spb, SFS_SB_LOCATION );

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert( spb.sp_magic == SFS_MAGIC );
	
	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);
	
	sd_cwd.sfd_ino = 1;		//init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}

void sfs_umount() {

	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}


void sfs_touch(const char* path)
{
	//skeleton implementation
	u_int32_t i, dir, bm, j, k, fi, fj;

	int noblock = 0;
	struct sfs_inode si;
	disk_read(&si, sd_cwd.sfd_ino);

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	//we assume that cwd is the root directory and root directory is empty which has . and .. only
	//unused DISK2.img satisfy these assumption
	//for new directory entry(for new file), we use cwd.sfi_direct[0] and offset 2
	//becasue cwd.sfi_directory[0] is already allocated, by .(offset 0) and ..(offset 1)
	//for new inode, we use block 6 
	// block 0: superblock,	block 1:root, 	block 2:bitmap 
	// block 3:bitmap,  	block 4:bitmap 	block 5:root.sfi_direct[0] 	block 6:unused
	//
	//if used DISK2.img is used, result is not defined

	//buffer for disk read
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], fd[SFS_DENTRYPERBLOCK];

	if (si.sfi_size == (SFS_NDIRECT * SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir))) {
		error_message("touch", path, -3);
		return;
	}

	for (i = 0; i < SFS_NDIRECT; i++) {
		if (si.sfi_direct[i] != SFS_NOINO) {
			disk_read(&fd, si.sfi_direct[i]);
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if (!strcmp(fd[j].sfd_name, path)) {
					if (fd[j].sfd_ino != SFS_NOINO) {
						error_message("touch", path, -6);
						return;
					}
				}
			}
		}
	}

	for (i = 0; i < SFS_NDIRECT; i++) {

		disk_read(&sd, si.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			if (sd[j].sfd_ino == SFS_NOINO) {
				fi = i;
				fj = j;
				noblock = 1;
				break;
			}
		}
		if (noblock) break;
	}
	noblock = 0;

	//allocate new block
	unsigned char bitmap[SFS_BLOCKSIZE];
	for (k = 0; k < SFS_BITBLOCKS(spb.sp_nblocks); k++) {
		disk_read(bitmap, SFS_MAP_LOCATION + k);
	
		for (i = 0; i < SFS_BLOCKSIZE; i++) {
			for (j = 0; j < 8; j++) {
				if (!BIT_CHECK(bitmap[i], j) && !noblock) {
					bm = SFS_BLOCKSIZE * 8 * k + i * 8 + j;
					BIT_SET(bitmap[i], j);
					disk_write(&bitmap, SFS_MAP_LOCATION + k);
					noblock = 1;
					break;
				}
			}
			if (noblock) break;
		}
		if (noblock) break;
	}

	if (!noblock) {
		error_message("touch", path, -4); return;
	}
	u_int32_t newbie_ino = bm;

	if (si.sfi_direct[fi]==SFS_NOINO) {
		si.sfi_direct[fi] = newbie_ino;
		noblock = 0;

		for (k = 0; k < SFS_BITBLOCKS(spb.sp_nblocks); k++) {
			disk_read(bitmap, SFS_MAP_LOCATION + k);

			for (i = 0; i < SFS_BLOCKSIZE; i++) {
				for (j = 0; j < 8; j++) {
					if (!BIT_CHECK(bitmap[i], j) && !noblock) {
						bm = SFS_BLOCKSIZE * 8 * k + i * 8 + j;
						BIT_SET(bitmap[i], j);
						disk_write(&bitmap, SFS_MAP_LOCATION + k);
						noblock = 1;
						break;
					}
				}
				if (noblock) break;
			}
			if (noblock) break;
		}

		if (!noblock) {
			error_message("touch", path, -4); return;
		}
		newbie_ino = bm;

		struct sfs_dir nsd[SFS_DENTRYPERBLOCK];
		nsd[0].sfd_ino = newbie_ino;
		for (i = 1; i < SFS_DENTRYPERBLOCK; i++) {
			nsd[i].sfd_ino = 0;
			strcpy(nsd[i].sfd_name, "");
		}
		strncpy(nsd[0].sfd_name, path, SFS_NAMELEN);

		disk_write(nsd, si.sfi_direct[fi]);

		si.sfi_size += sizeof(struct sfs_dir);
		disk_write(&si, sd_cwd.sfd_ino);

		struct sfs_inode newbie;

		bzero(&newbie, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
		newbie.sfi_size = 0;
		newbie.sfi_type = SFS_TYPE_FILE;
		
		disk_write(&newbie, newbie_ino);
		return;
	}
	
	sd[fj].sfd_ino = newbie_ino;
	strncpy(sd[fj].sfd_name, path, SFS_NAMELEN);

	disk_write(sd, si.sfi_direct[fi]);

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);

	struct sfs_inode newbie;

	bzero(&newbie, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;

	disk_write(&newbie, newbie_ino);
}

void sfs_cd(const char* path)
{
	if (path == NULL) {
		sd_cwd.sfd_ino = 1;		//init at root
		sd_cwd.sfd_name[0] = '/';
		sd_cwd.sfd_name[1] = '\0';
	}

	else {
		int i, j, k;
		struct sfs_dir lsd[SFS_DENTRYPERBLOCK];
		struct sfs_inode lsi, nsi;
		disk_read(&lsi, sd_cwd.sfd_ino);
		k = 0;

		for (i = 0; i <= (lsi.sfi_size / 513); i++) {
			disk_read(&lsd, lsi.sfi_direct[i]);
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if (!strcmp(lsd[j].sfd_name, path)) {
					if (lsd[j].sfd_ino != 0) {
						disk_read(&nsi, lsd[j].sfd_ino);
						if (nsi.sfi_type == SFS_TYPE_FILE) {
							error_message("cd", path, -2);
							k = 1;
							return;
						}
						else {
							sd_cwd.sfd_ino = lsd[j].sfd_ino;
							strcpy(sd_cwd.sfd_name, lsd[j].sfd_name);
							k = 1;
							break;
						}
					}

					else {
						error_message("cd", path, -1); return;
					}
				}
			}
		}
		if (!k) {
			error_message("cd", path, -1); return;
		}

	}
}

void sfs_ls(const char* path)
{
	u_int32_t i, j, k;
	struct sfs_dir lsd[SFS_DENTRYPERBLOCK], nsd[SFS_DENTRYPERBLOCK];
	struct sfs_inode lsi, nsi, msi;
	disk_read(&lsi, sd_cwd.sfd_ino);

	if (path == NULL) {
		for (i = 0; i <= (lsi.sfi_size / 513); i++) {	//512로하면 꽉찼을때 문제생김
			disk_read(&lsd, lsi.sfi_direct[i]);
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if (lsd[j].sfd_ino != 0) {
					disk_read(&nsi, lsd[j].sfd_ino);
					if (nsi.sfi_type == SFS_TYPE_DIR)
						printf("%s/\t", lsd[j].sfd_name);
					else
						printf("%s\t", lsd[j].sfd_name);
				}
			}
		}
		printf("\n");
	}

	else {
		k = 0;
		for (i = 0; i <= (lsi.sfi_size / 513); i++) {
			disk_read(&lsd, lsi.sfi_direct[i]);
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if (!strcmp(lsd[j].sfd_name,path)) {
					if (lsd[j].sfd_ino != 0) {
						disk_read(&nsi, lsd[j].sfd_ino);
						if (nsi.sfi_type == SFS_TYPE_DIR) {
							for (i = 0; i <= (nsi.sfi_size / 513); i++) {
								disk_read(&nsd, nsi.sfi_direct[i]);
								for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
									if (nsd[j].sfd_ino != 0) {
										disk_read(&msi, nsd[j].sfd_ino);
										if (msi.sfi_type == SFS_TYPE_DIR)
											printf("%s/\t", nsd[j].sfd_name);
										else
											printf("%s\t", nsd[j].sfd_name);
									}
								}
							}
							k = 1; printf("\n");
						}

						else
							printf("%s\n", lsd[j].sfd_name);
						k = 1;
						break;
					}
					else {
						error_message("ls", path, -1); return;
					}
				}
			}
		}
		if (!k) {
			error_message("ls", path, -1); return;
		}
	}
}

void sfs_mkdir(const char* org_path) 
{
	//skeleton implementation
	u_int32_t i, bm, j, k, fi, fj;
	int noblock = 0;
	struct sfs_inode si;
	disk_read(&si, sd_cwd.sfd_ino);

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	struct sfs_dir sd[SFS_DENTRYPERBLOCK], fd[SFS_DENTRYPERBLOCK];

	if (si.sfi_size == (SFS_NDIRECT * SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir))) {
		error_message("mkdir", org_path, -3);
		return;
	}

	for (i = 0; i < SFS_NDIRECT; i++) {
		if (si.sfi_direct[i] != SFS_NOINO) {
			disk_read(&fd, si.sfi_direct[i]);
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if (!strcmp(fd[j].sfd_name, org_path)) {
					if (fd[j].sfd_ino != SFS_NOINO) {
						error_message("mkdir", org_path, -6);
						return;
					}
				}
			}
		}
	}

	for (i = 0; i < SFS_NDIRECT; i++) {

		disk_read(&sd, si.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			if (sd[j].sfd_ino == SFS_NOINO) {
				fi = i;
				fj = j;
				noblock = 1;
				break;
			}
		}
		if (noblock) break;
	}
	noblock = 0;

	//allocate new block
	unsigned char bitmap[SFS_BLOCKSIZE];

	for (k = 0; k < SFS_BITBLOCKS(spb.sp_nblocks); k++) {
		disk_read(bitmap, SFS_MAP_LOCATION + k);

		for (i = 0; i < SFS_BLOCKSIZE; i++) {
			for (j = 0; j < 8; j++) {
				if (!BIT_CHECK(bitmap[i], j) && !noblock) {
					bm = SFS_BLOCKSIZE * 8 * k + i * 8 + j;
					BIT_SET(bitmap[i], j);
					disk_write(&bitmap, SFS_MAP_LOCATION + k);
					noblock = 1;
					break;
				}
			}
			if (noblock) break;
		}
		if (noblock) break;
	}

	if (!noblock) {
		error_message("mkdir", org_path, -4); return;
	}
	u_int32_t newbie_ino = bm;

	if (si.sfi_direct[fi]==SFS_NOINO) {

		si.sfi_direct[fi] = newbie_ino;
		noblock = 0;

		for (k = 0; k < SFS_BITBLOCKS(spb.sp_nblocks); k++) {
			disk_read(bitmap, SFS_MAP_LOCATION + k);

			for (i = 0; i < SFS_BLOCKSIZE; i++) {
				for (j = 0; j < 8; j++) {
					if (!BIT_CHECK(bitmap[i], j) && !noblock) {
						bm = SFS_BLOCKSIZE * 8 * k + i * 8 + j;
						BIT_SET(bitmap[i], j);
						disk_write(&bitmap, SFS_MAP_LOCATION + k);
						noblock = 1;
						break;
					}
				}
				if (noblock) break;
			}
			if (noblock) break;
		}

		if (!noblock) {
			error_message("mkdir", org_path, -4); return;
		}
		newbie_ino = bm;
		struct sfs_dir asd[SFS_DENTRYPERBLOCK];
		asd[0].sfd_ino = newbie_ino;
		for (i = 1; i < SFS_DENTRYPERBLOCK; i++)
			asd[i].sfd_ino = 0;
		strncpy(asd[0].sfd_name, org_path, SFS_NAMELEN);

		disk_write(asd, si.sfi_direct[fi]);

		si.sfi_size += sizeof(struct sfs_dir);
		disk_write(&si, sd_cwd.sfd_ino);

		struct sfs_inode newbie;

		bzero(&newbie, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
		newbie.sfi_size = 2 * sizeof(struct sfs_dir);
		newbie.sfi_type = SFS_TYPE_DIR;
		u_int32_t newbno = newbie_ino;

		noblock = 0;
		for (k = 0; k < SFS_BITBLOCKS(spb.sp_nblocks); k++) {
			disk_read(bitmap, SFS_MAP_LOCATION + k);

			for (i = 0; i < SFS_BLOCKSIZE; i++) {
				for (j = 0; j < 8; j++) {
					if (!BIT_CHECK(bitmap[i], j) && !noblock) {
						bm = SFS_BLOCKSIZE * 8 * k + i * 8 + j;
						BIT_SET(bitmap[i], j);
						disk_write(&bitmap, SFS_MAP_LOCATION + k);
						noblock = 1;
						break;
					}
				}
				if (noblock) break;
			}
			if (noblock) break;
		}

		if (!noblock) {
			error_message("mkdir", org_path, -4); return;
		}
		newbie_ino = bm;

		newbie.sfi_direct[0] = newbie_ino;
		disk_write(&newbie, newbno);
		struct sfs_dir msd[SFS_DENTRYPERBLOCK];
		msd[0].sfd_ino = newbno;
		strcpy(msd[0].sfd_name, ".");
		msd[1].sfd_ino = sd_cwd.sfd_ino;
		strcpy(msd[1].sfd_name, "..");
		for (i = 2; i < SFS_DENTRYPERBLOCK; i++) {
			msd[i].sfd_ino = 0;
			strcpy(msd[i].sfd_name, "");
		}
		disk_write(msd, newbie.sfi_direct[0]);
		return;
	}

	sd[fj].sfd_ino = newbie_ino;
	strncpy(sd[fj].sfd_name, org_path, SFS_NAMELEN);

	disk_write(sd, si.sfi_direct[fi]);

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);

	struct sfs_inode newbie;

	bzero(&newbie, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 2 * sizeof(struct sfs_dir);
	newbie.sfi_type = SFS_TYPE_DIR;
	u_int32_t newbno = newbie_ino;

	noblock = 0;
	for (k = 0; k < SFS_BITBLOCKS(spb.sp_nblocks); k++) {
		disk_read(bitmap, SFS_MAP_LOCATION + k);

		for (i = 0; i < SFS_BLOCKSIZE; i++) {
			for (j = 0; j < 8; j++) {
				if (!BIT_CHECK(bitmap[i], j) && !noblock) {
					bm = SFS_BLOCKSIZE * 8 * k + i * 8 + j;
					BIT_SET(bitmap[i], j);
					disk_write(&bitmap, SFS_MAP_LOCATION + k);
					noblock = 1;
					break;
				}
			}
			if (noblock) break;
		}
		if (noblock) break;
	}

	if (!noblock) {
		error_message("mkdir", org_path, -4); return;
	}
	newbie_ino = bm;

	newbie.sfi_direct[0] = newbie_ino;
	disk_write(&newbie, newbno);
	struct sfs_dir msd[SFS_DENTRYPERBLOCK];
	msd[0].sfd_ino = newbno;
	strcpy(msd[0].sfd_name, ".");
	msd[1].sfd_ino = sd_cwd.sfd_ino;
	strcpy(msd[1].sfd_name, "..");
	for (i = 2; i < SFS_DENTRYPERBLOCK; i++) {
		msd[i].sfd_ino = SFS_NOINO;
		strcpy(msd[i].sfd_name, "");
	}
	disk_write(msd, newbie.sfi_direct[0]);
}

void sfs_rmdir(const char* org_path) 
{
	//skeleton implementation
	u_int32_t i, bm, j, k, fi, fj, ai, aj, ak, ci, cj, ck;
	int bk, noblock = 0;
	struct sfs_inode si, ni, ti;
	disk_read(&si, sd_cwd.sfd_ino);

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	struct sfs_dir sd[SFS_DENTRYPERBLOCK], fd[SFS_DENTRYPERBLOCK];

	for (i = 0; i < SFS_NDIRECT; i++) {
		if (si.sfi_direct[i] != SFS_NOINO) {
			disk_read(&fd, si.sfi_direct[i]);
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if (!strcmp(fd[j].sfd_name, org_path)) {
					fi = i;
					fj = j;
					bk = 1;
					break;
				}
			}
		}
		if (bk) break;
	}
	if (!bk) {
		error_message("rmdir", org_path, -1);
		return;
	}
	if (!strcmp(fd[j].sfd_name, ".")) {
		error_message("rmdir", org_path, -8);
		return;
	}

	disk_read(&ti, fd[j].sfd_ino);
	if (ti.sfi_type != SFS_TYPE_DIR) {
		error_message("rmdir", org_path, -5);
		return;
	}
	if (ti.sfi_size > 2 * sizeof(struct sfs_dir)) {
		error_message("rmdir", org_path, -7);
		return;
	}

	ak = fd[fj].sfd_ino / (SFS_BLOCKSIZE * 8 + 1);
	ai = (fd[fj].sfd_ino - (ak * SFS_BLOCKSIZE * 8)) / 8;
	aj = (fd[fj].sfd_ino % (CHAR_BIT));
	unsigned char bitmap[SFS_BLOCKSIZE], bitmap2[SFS_BLOCKSIZE];
	disk_read(bitmap, SFS_MAP_LOCATION + ak);
	BIT_CLEAR(bitmap[ai], aj);
	disk_write(bitmap, SFS_MAP_LOCATION + ak);

	disk_read(&ni, fd[fj].sfd_ino);
	ck = ni.sfi_direct[0]/ (SFS_BLOCKSIZE * 8 + 1);
	ci = (ni.sfi_direct[0] - (ck * SFS_BLOCKSIZE * 8)) / 8;
	cj = (ni.sfi_direct[0] % (CHAR_BIT));
	disk_read(bitmap2, SFS_MAP_LOCATION + ck);
	BIT_CLEAR(bitmap2[ci], cj);
	disk_write(bitmap2, SFS_MAP_LOCATION + ck);

	ni.sfi_direct[0] = SFS_NOINO;
	disk_write(&ni, fd[fj].sfd_ino);
	fd[fj].sfd_ino = SFS_NOINO;
	si.sfi_size -= sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);
	disk_write(fd, si.sfi_direct[fi]);

}

void sfs_mv(const char* src_name, const char* dst_name) 
{
	//skeleton implementation
	u_int32_t i, bm, j, k, fi, fj;
	int noblock, bk = 0;
	struct sfs_inode si;
	disk_read(&si, sd_cwd.sfd_ino);

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	struct sfs_dir sd[SFS_DENTRYPERBLOCK], fd[SFS_DENTRYPERBLOCK];

	if (sizeof(src_name) > SFS_NAMELEN || !strcmp(src_name, ".") || !strcmp(src_name, "..")) {
		error_message("mv", src_name, -8);
	}
	if (sizeof(dst_name) > SFS_NAMELEN || !strcmp(dst_name, ".") || !strcmp(dst_name, "..")) {
		error_message("mv", dst_name, -8);
	}

	for (i = 0; i < SFS_NDIRECT; i++) {
		if (si.sfi_direct[i] != SFS_NOINO) {
			disk_read(&fd, si.sfi_direct[i]);
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if (!strcmp(fd[j].sfd_name, dst_name)) {
					if (fd[j].sfd_ino != SFS_NOINO) {
						error_message("mv", dst_name, -6);
						return;
					}
				}

				else if (!strcmp(fd[j].sfd_name, src_name)) {
					fi = i;
					fj = j;
					bk = 1;
				}
			}
		}
	}
	if (!bk) {
		error_message("mv", src_name, -1);
		return;
	}

	strcpy(fd[fj].sfd_name, dst_name);
	disk_write(fd, si.sfi_direct[fi]);
}

void sfs_rm(const char* path) 
{
	//skeleton implementation
	u_int32_t i, dir, bm, j, k, fi, fj, ai, aj, ak, ci, cj, ck;
	int bk, noblock = 0;
	struct sfs_inode si, ni, ti;
	disk_read(&si, sd_cwd.sfd_ino);

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	struct sfs_dir sd[SFS_DENTRYPERBLOCK], fd[SFS_DENTRYPERBLOCK];

	for (i = 0; i < SFS_NDIRECT; i++) {
		if (si.sfi_direct[i] != SFS_NOINO) {
			disk_read(&fd, si.sfi_direct[i]);
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if (!strcmp(fd[j].sfd_name, path)) {
					fi = i;
					fj = j;
					bk = 1;
					break;
				}
			}
		}
		if (bk) break;
	}
	if (!bk) {
		error_message("rm", path, -1);
		return;
	}

	disk_read(&ti, fd[j].sfd_ino);
	if (ti.sfi_type == SFS_TYPE_DIR) {
		error_message("rm", path, -9);
		return;
	}

	ak = fd[fj].sfd_ino / (SFS_BLOCKSIZE * 8 + 1);
	ai = (fd[fj].sfd_ino - (ak * SFS_BLOCKSIZE * 8)) / 8;
	aj = (fd[fj].sfd_ino % (CHAR_BIT));
	unsigned char bitmap[SFS_BLOCKSIZE], bitmap2[SFS_BLOCKSIZE];
	disk_read(bitmap, SFS_MAP_LOCATION + ak);
	BIT_CLEAR(bitmap[ai], aj);
	disk_write(bitmap, SFS_MAP_LOCATION + ak);

	disk_read(&ni, fd[fj].sfd_ino);
	
	for (i = 0; i < SFS_NDIRECT; i++) {
		if (ni.sfi_direct[i] != SFS_NOINO) {
			ck = ni.sfi_direct[i] / (SFS_BLOCKSIZE * 8 + 1);
			ci = (ni.sfi_direct[i] - (ck * SFS_BLOCKSIZE * 8)) / 8;
			cj = (ni.sfi_direct[i] % (CHAR_BIT));
			disk_read(bitmap2, SFS_MAP_LOCATION + ck);
			BIT_CLEAR(bitmap2[ci], cj);
			disk_write(bitmap2, SFS_MAP_LOCATION + ck);
			ni.sfi_direct[i] = SFS_NOINO;
		}
	}

	if (ni.sfi_indirect != SFS_NOINO) {
		u_int32_t ind[SFS_DBPERIDB], dk, di, dj;
		disk_read(ind, ni.sfi_indirect);

		for (i = 0; i < SFS_DBPERIDB; i++) {
			if (ind[i] != SFS_NOINO) {
				dk = ind[i] / (SFS_BLOCKSIZE * 8 + 1);
				di = (ind[i] - (dk * SFS_BLOCKSIZE * 8)) / 8;
				dj = (ind[i] % (CHAR_BIT));
				disk_read(bitmap2, SFS_MAP_LOCATION + dk);
				BIT_CLEAR(bitmap2[di], dj);
				disk_write(bitmap2, SFS_MAP_LOCATION + dk);
				ind[i] = 0;
			}
		}

		dk = ni.sfi_indirect / (SFS_BLOCKSIZE * 8 + 1);
		di = (ni.sfi_indirect - (dk * SFS_BLOCKSIZE * 8)) / 8;
		dj = (ni.sfi_indirect % (CHAR_BIT));
		disk_read(bitmap2, SFS_MAP_LOCATION + dk);
		BIT_CLEAR(bitmap2[di], dj);
		disk_write(bitmap2, SFS_MAP_LOCATION + dk);

	}

	disk_write(&ni, fd[fj].sfd_ino);
	fd[fj].sfd_ino = SFS_NOINO;
	si.sfi_size -= sizeof(struct sfs_dir);
	disk_write(&si, sd_cwd.sfd_ino);
	disk_write(fd, si.sfi_direct[fi]);

}

void sfs_cpin(const char* local_path, const char* path)
{
	
	u_int32_t i, dir, bm, j, k, fsize, realno, fi, fj;
	int noblock = 0;
	struct sfs_inode si;
	disk_read(&si, sd_cwd.sfd_ino);
	FILE* fp;
	u_int32_t ind[SFS_DBPERIDB];

	for (i = 0; i < SFS_DBPERIDB; i++)
		ind[i] = 0;

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	struct sfs_dir sd[SFS_DENTRYPERBLOCK], fd[SFS_DENTRYPERBLOCK];
	unsigned char bitmap[SFS_BLOCKSIZE], readbuf[SFS_BLOCKSIZE];

	if (si.sfi_size == (SFS_NDIRECT * SFS_DENTRYPERBLOCK * sizeof(struct sfs_dir))) {
		error_message("cpin", path, -3);
		return;
	}

	fp = fopen(path, "r");
	if (!fp) {
		error_message("cpin", path, -11);
		return;
	}

	fseek(fp, 0, SEEK_END);
	if ((fsize = ftell(fp)) > SFS_NDIRECT * SFS_BLOCKSIZE + SFS_DBPERIDB * SFS_BLOCKSIZE) {
		error_message("cpin", path, -12);
		return;
	}

	for (i = 0; i < SFS_NDIRECT; i++) {
		if (si.sfi_direct[i] != SFS_NOINO) {
			disk_read(&fd, si.sfi_direct[i]);
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if (!strcmp(fd[j].sfd_name, local_path)) {
					if (fd[j].sfd_ino != SFS_NOINO) {
						error_message("cpin", local_path, -6);
						return;
					}
				}
			}
		}
	}

	for (i = 0; i < SFS_NDIRECT; i++) {

		disk_read(&sd, si.sfi_direct[i]);
		for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
			if (sd[j].sfd_ino == SFS_NOINO) {
				fi = i;
				fj = j;
				noblock = 1;
				break;
			}
		}
		if (noblock) break;
	}
	noblock = 0;

	//allocate new block

	for (k = 0; k < SFS_BITBLOCKS(spb.sp_nblocks); k++) {
		disk_read(bitmap, SFS_MAP_LOCATION + k);

		for (i = 0; i < SFS_BLOCKSIZE; i++) {
			for (j = 0; j < 8; j++) {
				if (!BIT_CHECK(bitmap[i], j) && !noblock) {
					bm = SFS_BLOCKSIZE * 8 * k + i * 8 + j;
					BIT_SET(bitmap[i], j);
					disk_write(&bitmap, SFS_MAP_LOCATION + k);
					noblock = 1;
					break;
				}
			}
			if (noblock) break;
		}
		if (noblock) break;
	}

	if (!noblock) {
		error_message("cpin", local_path, -4); return;
	}
	u_int32_t newbie_ino = bm;

	if (si.sfi_direct[fi] == SFS_NOINO) {
		si.sfi_direct[fi] = newbie_ino;
		noblock = 0;
		for (k = 0; k < SFS_BITBLOCKS(spb.sp_nblocks); k++) {
			disk_read(bitmap, SFS_MAP_LOCATION + k);

			for (i = 0; i < SFS_BLOCKSIZE; i++) {
				for (j = 0; j < 8; j++) {
					if (!BIT_CHECK(bitmap[i], j) && !noblock) {
						bm = SFS_BLOCKSIZE * 8 * k + i * 8 + j;
						BIT_SET(bitmap[i], j);
						disk_write(&bitmap, SFS_MAP_LOCATION + k);
						noblock = 1;
						break;
					}
				}
				if (noblock) break;
			}
			if (noblock) break;
		}

		if (!noblock) {
			error_message("cpin", path, -4); return;
		}
		newbie_ino = bm;
		noblock = 0;

		struct sfs_dir nsd[SFS_DENTRYPERBLOCK];
		nsd[0].sfd_ino = newbie_ino;
		for (i = 1; i < SFS_DENTRYPERBLOCK; i++) {
			nsd[i].sfd_ino = 0;
			strcpy(nsd[i].sfd_name, "");
		}
		strncpy(nsd[0].sfd_name, local_path, SFS_NAMELEN);

		si.sfi_size += sizeof(struct sfs_dir);
		disk_write(nsd, si.sfi_direct[fi]);
		disk_write(&si, sd_cwd.sfd_ino);
	}
	else {
		sd[fj].sfd_ino = newbie_ino;
		strncpy(sd[fj].sfd_name, local_path, SFS_NAMELEN);

		disk_write(sd, si.sfi_direct[fi]);

		si.sfi_size += sizeof(struct sfs_dir);
		disk_write(&si, sd_cwd.sfd_ino);
	}
	struct sfs_inode newbie;

	bzero(&newbie, SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = fsize;
	newbie.sfi_type = SFS_TYPE_FILE;
	disk_write(&newbie, newbie_ino);

	u_int32_t headino = newbie_ino;
	fp = fopen(path, "r");

	struct sfs_inode yi;
	disk_read(&yi, headino);
	int cnt = 0;
	while (1) {		//fsize만큼 채우던가 다이렉트갯수가 마지노 
		if (cnt >= SFS_NDIRECT) break;
		if (fsize == 0) break;
	
		for (i = 0; i < SFS_NDIRECT; i++) {
			if (yi.sfi_direct[i] == SFS_NOINO) {
				fi = i;
				break;
			}

		}
		noblock = 0;

		for (k = 0; k < SFS_BITBLOCKS(spb.sp_nblocks); k++) {
			disk_read(bitmap, SFS_MAP_LOCATION + k);

			for (i = 0; i < SFS_BLOCKSIZE; i++) {
				for (j = 0; j < 8; j++) {
					if (!BIT_CHECK(bitmap[i], j) && !noblock) {
						bm = SFS_BLOCKSIZE * 8 * k + i * 8 + j;
						BIT_SET(bitmap[i], j);
						disk_write(&bitmap, SFS_MAP_LOCATION + k);
						noblock = 1;
						break;
					}
				}
				if (noblock) break;
			}
			if (noblock) break;
		}

		if (!noblock) {
			struct sfs_inode ss;
			disk_read(&ss, headino);
			ss.sfi_size -= fsize;
			disk_write(&ss, headino);
			error_message("cpin", local_path, -4); return;
		}
		newbie_ino = bm;
		yi.sfi_direct[fi] = newbie_ino;
		if (fsize < SFS_BLOCKSIZE) {
			fread(readbuf, sizeof(unsigned char), fsize, fp);
			disk_write(readbuf, yi.sfi_direct[fi]);

			fsize = 0;
		}

		else {
			fread(readbuf, sizeof(unsigned char), SFS_BLOCKSIZE, fp);
			disk_write(readbuf, yi.sfi_direct[fi]);
			fsize -= SFS_BLOCKSIZE;
		}
		disk_write(&yi, headino);
		cnt++;
	}

	if (fsize > 0) {	//indirect
		int cm = 0;
		//allocate new block

		noblock = 0;
		for (k = 0; k < SFS_BITBLOCKS(spb.sp_nblocks); k++) {
			disk_read(bitmap, SFS_MAP_LOCATION + k);

			for (i = 0; i < SFS_BLOCKSIZE; i++) {
				for (j = 0; j < 8; j++) {
					if (!BIT_CHECK(bitmap[i], j) && !noblock) {
						cm = SFS_BLOCKSIZE * 8 * k + i * 8 + j;
						BIT_SET(bitmap[i], j);
						disk_write(&bitmap, SFS_MAP_LOCATION + k);
						noblock = 1;
						break;
					}
				}
				if (noblock) break;
			}
			if (noblock) break;
		}

		if (!noblock) {
			struct sfs_inode ss;
			disk_read(&ss, headino);
			ss.sfi_size -= fsize;
			disk_write(&ss, headino);
			error_message("cpin", local_path, -4); return;
		}
		newbie_ino = cm;
		disk_read(&yi, headino);
		yi.sfi_indirect = newbie_ino;
		disk_write(&yi, headino);

		int a;
		for (a = 0; a < SFS_DBPERIDB; a++) {
			if (fsize == 0) { fclose(fp); break; }

			noblock = 0;

			//allocate new block

			for (k = 0; k < SFS_BITBLOCKS(spb.sp_nblocks); k++) {
				disk_read(bitmap, SFS_MAP_LOCATION + k);

				for (i = 0; i < SFS_BLOCKSIZE; i++) {
					for (j = 0; j < 8; j++) {
						if (!BIT_CHECK(bitmap[i], j) && !noblock) {
							bm = SFS_BLOCKSIZE * 8 * k + i * 8 + j;
							BIT_SET(bitmap[i], j);
							disk_write(&bitmap, SFS_MAP_LOCATION + k);
							noblock = 1;
							break;
						}
					}
					if (noblock) break;
				}
				if (noblock) break;
			}

			if (!noblock) {
				struct sfs_inode ss;
				disk_read(&ss, headino);
				ss.sfi_size -= fsize;
				disk_write(&ss, headino);
				error_message("cpin", local_path, -4); return;
			}
			noblock = 0;
			newbie_ino = bm;

			ind[a] = newbie_ino;

			if (fsize < SFS_BLOCKSIZE) {
				fread(readbuf, sizeof(unsigned char), fsize, fp);
				disk_write(readbuf, newbie_ino);
				fsize = 0;
			}

			else {
				fread(readbuf, sizeof(unsigned char), SFS_BLOCKSIZE, fp);
				disk_write(readbuf, newbie_ino);
				fsize -= SFS_BLOCKSIZE;
			}
			disk_write(ind, yi.sfi_indirect);
		}
	}

	else fclose(fp);
}


void sfs_cpout(const char* local_path, const char* path)
{
	u_int32_t i, dir, bm, j, k, fsize, realno, fi, fj, headino;
	int noblock, bk = 0;
	struct sfs_inode si, ni;
	disk_read(&si, sd_cwd.sfd_ino);
	FILE* fp;
	u_int32_t ind[SFS_DBPERIDB];

	for (i = 0; i < SFS_DBPERIDB; i++)
		ind[i] = 0;

	//for consistency
	assert(si.sfi_type == SFS_TYPE_DIR);

	struct sfs_dir sd[SFS_DENTRYPERBLOCK], fd[SFS_DENTRYPERBLOCK];
	unsigned char writebuf[SFS_BLOCKSIZE];

	fp = fopen(path, "r");

	if (fp) {
		error_message("cpout", path, -6);
		return;
	}

	for (i = 0; i < SFS_NDIRECT; i++) {
		if (si.sfi_direct[i] != SFS_NOINO) {
			disk_read(&fd, si.sfi_direct[i]);
			for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
				if (!strcmp(fd[j].sfd_name, local_path)) {
					fi = i;
					fj = j;
					bk = 1;
					break;
				}
			}
		}
		if (bk) break;
	}
	if (!bk) {
		error_message("cpout", local_path, -1);
		return;
	}

	fp = fopen(path, "w");
	struct sfs_inode yi;
	struct sfs_dir nd[SFS_DENTRYPERBLOCK];
	disk_read(nd, si.sfi_direct[fi]);
	disk_read(&yi, nd[fj].sfd_ino);
	headino = si.sfi_direct[fi];

	fsize = yi.sfi_size;
	int cnt = 0;

	while (1) {
		if (fsize == 0) break;
		if (cnt >= SFS_NDIRECT) break;

		for (i = 0; i < SFS_NDIRECT; i++) {
			if (yi.sfi_direct[i] != SFS_NOINO) {
				fi = i;
				break;
			}

		}
		noblock = 0;
		
		disk_read(writebuf, yi.sfi_direct[fi]);

		if (fsize < SFS_BLOCKSIZE) {
			fwrite(writebuf, sizeof(unsigned char), fsize, fp);
			fsize = 0;
		}

		else {
			fwrite(writebuf, sizeof(unsigned char), SFS_BLOCKSIZE, fp);
			fsize -= SFS_BLOCKSIZE;
		}
		cnt++;
	}

	if (fsize > 0) {
		struct sfs_inode ni;
		disk_read(ind, yi.sfi_indirect);

		for (i = 0; i < SFS_DBPERIDB; i++) {
			if (fsize == 0)break;

			if (ind[i] != SFS_NOINO) {
				disk_read(writebuf, ind[i]);

				if (fsize < SFS_BLOCKSIZE) {
					fwrite(writebuf, sizeof(unsigned char), fsize, fp);
					fsize = 0;
				}

				else {
					fwrite(writebuf, sizeof(unsigned char), SFS_BLOCKSIZE, fp);
					fsize -= SFS_BLOCKSIZE;
				}
			}
		}
	}

	fclose(fp);
}

void dump_inode(struct sfs_inode inode) {
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for(i=0; i < SFS_NDIRECT; i++) {
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d",inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}

}

void dump_directory(struct sfs_dir dir_entry[]) {
	int i;
	struct sfs_inode inode;
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		printf("%d %s\n",dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump() {
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n",sd_cwd.sfd_ino,sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");

}
