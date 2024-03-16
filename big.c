#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <fuse/fuse_opt.h>
#include <fuse/fuse_lowlevel.h>
#include <lkl.h>
#include <lkl_host.h>

#define LKLFUSE_VERSION "0.1"

struct lklfuse {
	const char *file;
	const char *log;
	const char *type;
	const char *opts;
	struct lkl_disk disk;
	int disk_id;
	int part;
	int ro;
	int mb;
} lklfuse = {
	.mb = 64,
};

#define LKLFUSE_OPT(t, p, v) { t, offsetof(struct lklfuse, p), v }

enum {
	KEY_HELP,
	KEY_VERSION,
};

static struct fuse_opt lklfuse_opts[] = {
	LKLFUSE_OPT("log=%s", log, 0),
	LKLFUSE_OPT("type=%s", type, 0),
	LKLFUSE_OPT("mb=%d", mb, 0),
	LKLFUSE_OPT("opts=%s", opts, 0),
	LKLFUSE_OPT("part=%d", part, 0),
	FUSE_OPT_KEY("-h", KEY_HELP),
	FUSE_OPT_KEY("--help", KEY_HELP),
	FUSE_OPT_KEY("-V",             KEY_VERSION),
	FUSE_OPT_KEY("--version",      KEY_VERSION),
	FUSE_OPT_END
};

static void usage(void)
{
	printf(
"usage: lklfuse file mountpoint [options]\n"
"\n"
"general options:\n"
"    -o opt,[opt...]        mount options\n"
"    -h   --help            print help\n"
"    -V   --version         print version\n"
"\n"
"lklfuse options:\n"
"    -o log=FILE            log file\n"
"    -o type=fstype         filesystem type\n"
"    -o mb=memory in mb     ammount of memory to allocate\n"
"    -o part=parition       partition to mount\n"
"    -o opts=options        mount options (use \\ to escape , and =)\n"
);
}

static int lklfuse_opt_proc(void *data, const char *arg, int key,
			  struct fuse_args *args)
{
	switch (key) {
	case FUSE_OPT_KEY_OPT:
		if (strcmp(arg, "ro") == 0)
			lklfuse.ro = 1;
		return 1;

	case FUSE_OPT_KEY_NONOPT:
		if (!lklfuse.file) {
			lklfuse.file = strdup(arg);
			return 0;
		}
		return 1;

	case KEY_HELP:
		usage();
		fuse_opt_add_arg(args, "-ho");
		fuse_main(args->argc, args->argv, NULL, NULL);
		exit(1);

	case KEY_VERSION:
		printf("lklfuse version %s\n", LKLFUSE_VERSION);
		fuse_opt_add_arg(args, "--version");
		fuse_main(args->argc, args->argv, NULL, NULL);
		exit(0);

	default:
		fprintf(stderr, "internal error\n");
		abort();
	}
}

static void lklfuse_xlat_stat(const struct lkl_stat *in, struct stat *st)
{
	st->st_dev = in->st_dev;
	st->st_ino = in->st_ino;
	st->st_mode = in->st_mode;
	st->st_nlink = in->st_nlink;
	st->st_uid = in->st_uid;
	st->st_gid = in->st_gid;
	st->st_rdev = in->st_rdev;
	st->st_size = in->st_size;
	st->st_blksize = in->st_blksize;
	st->st_blocks = in->st_blocks;
	st->st_atim.tv_sec = in->lkl_st_atime;
	st->st_atim.tv_nsec = in->st_atime_nsec;
	st->st_mtim.tv_sec = in->lkl_st_mtime;
	st->st_mtim.tv_nsec = in->st_mtime_nsec;
	st->st_ctim.tv_sec = in->lkl_st_ctime;
	st->st_ctim.tv_nsec = in->st_ctime_nsec;
}

static int lklfuse_fgetattr(const char *path, struct stat *st,
	                    struct fuse_file_info *fi)
{
	long ret;
	struct lkl_stat lkl_stat;

	ret = lkl_sys_fstat(fi->fh, &lkl_stat);
	if (ret)
		return ret;

	lklfuse_xlat_stat(&lkl_stat, st);
	return 0;
}

static int lklfuse_getattr(const char *path, struct stat *st)
{
	long ret;
	struct lkl_stat lkl_stat;

	ret = lkl_sys_lstat(path, &lkl_stat);
	if (ret)
		return ret;

	lklfuse_xlat_stat(&lkl_stat, st);
	return 0;
}

static int lklfuse_readlink(const char *path, char *buf, size_t len)
{
	long ret;

	ret = lkl_sys_readlink(path, buf, len);
	if (ret < 0)
		return ret;

	if ((size_t)ret == len)
		ret = len - 1;

	buf[ret] = 0;

	return 0;
}

static int lklfuse_mknod(const char *path, mode_t mode, dev_t dev)
{
	return lkl_sys_mknod(path, mode, dev);
}

static int lklfuse_mkdir(const char *path, mode_t mode)
{
	return lkl_sys_mkdir(path, mode);
}

static int lklfuse_unlink(const char *path)
{
	return lkl_sys_unlink(path);
}

static int lklfuse_rmdir(const char *path)
{
	return lkl_sys_rmdir(path);
}

static int lklfuse_symlink(const char *oldname, const char *newname)
{
	return lkl_sys_symlink(oldname, newname);
}


static int lklfuse_rename(const char *oldname, const char *newname)
{
	return lkl_sys_rename(oldname, newname);
}

static int lklfuse_link(const char *oldname, const char *newname)
{
	return lkl_sys_link(oldname, newname);
}

static int lklfuse_chmod(const char *path, mode_t mode)
{
	return lkl_sys_chmod(path, mode);
}


static int lklfuse_chown(const char *path, uid_t uid, gid_t gid)
{
	return lkl_sys_chown(path, uid, gid);
}

static int lklfuse_truncate(const char *path, off_t off)
{
	return lkl_sys_truncate(path, off);
}

static int lklfuse_open3(const char *path, bool create, mode_t mode,
	                 struct fuse_file_info *fi)
{
	long ret;
	int flags;

	if ((fi->flags & O_ACCMODE) == O_RDONLY)
		flags = LKL_O_RDONLY;
	else if ((fi->flags & O_ACCMODE) == O_WRONLY)
		flags = LKL_O_WRONLY;
	else if ((fi->flags & O_ACCMODE) == O_RDWR)
		flags = LKL_O_RDWR;
	else
		return -EINVAL;

	if (create)
		flags |= LKL_O_CREAT;

	ret = lkl_sys_open(path, flags, mode);
	if (ret < 0)
		return ret;

	fi->fh = ret;

	return 0;
}

static int lklfuse_create(const char *path, mode_t mode,
	                  struct fuse_file_info *fi)
{
	return lklfuse_open3(path, true, mode, fi);
}

static int lklfuse_open(const char *path, struct fuse_file_info *fi)
{
	return lklfuse_open3(path, false, 0, fi);
}

static int lklfuse_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	long ret;
	ssize_t orig_size = size;

	do {
		ret = lkl_sys_pread64(fi->fh, buf, size, offset);
		if (ret <= 0)
			break;
		size -= ret;
		offset += ret;
		buf += ret;
	} while (size > 0);

	return ret < 0 ? ret : orig_size - (ssize_t)size;

}

static int lklfuse_write(const char *path, const char *buf, size_t size,
		       off_t offset, struct fuse_file_info *fi)
{
	long ret;
	ssize_t orig_size = size;

	do {
		ret = lkl_sys_pwrite64(fi->fh, buf, size, offset);
		if (ret <= 0)
			break;
		size -= ret;
		offset += ret;
		buf += ret;
	} while (size > 0);

	return ret < 0 ? ret : orig_size - (ssize_t)size;
}


static int lklfuse_statfs(const char *path, struct statvfs *stat)
{
	long ret;
	struct lkl_statfs lkl_statfs;

	ret = lkl_sys_statfs(path, &lkl_statfs);
	if (ret < 0)
		return ret;

	stat->f_bsize = lkl_statfs.f_bsize;
	stat->f_frsize = lkl_statfs.f_frsize;
	stat->f_blocks = lkl_statfs.f_blocks;
	stat->f_bfree = lkl_statfs.f_bfree;
	stat->f_bavail = lkl_statfs.f_bavail;
	stat->f_files = lkl_statfs.f_files;
	stat->f_ffree = lkl_statfs.f_ffree;
	stat->f_favail = stat->f_ffree;
	stat->f_fsid = *(unsigned long *)&lkl_statfs.f_fsid.val[0];
	stat->f_flag = lkl_statfs.f_flags;
	stat->f_namemax = lkl_statfs.f_namelen;

	return 0;
}

static int lklfuse_flush(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int lklfuse_release(const char *path, struct fuse_file_info *fi)
{
	return lkl_sys_close(fi->fh);
}

static int lklfuse_fsync(const char *path, int datasync,
		       struct fuse_file_info *fi)
{
	if (datasync)
		return lkl_sys_fdatasync(fi->fh);
	else
		return lkl_sys_fsync(fi->fh);
}

static int lklfuse_setxattr(const char *path, const char *name, const char *val,
		   size_t size, int flags)
{
	return lkl_sys_setxattr(path, name, val, size, flags);
}

static int lklfuse_getxattr(const char *path, const char *name, char *val,
			  size_t size)
{
	return lkl_sys_getxattr(path, name, val, size);
}

static int lklfuse_listxattr(const char *path, char *list, size_t size)
{
	return lkl_sys_listxattr(path, list, size);
}

static int lklfuse_removexattr(const char *path, const char *name)
{
	return lkl_sys_removexattr(path, name);
}

static int lklfuse_opendir(const char *path, struct fuse_file_info *fi)
{
	struct lkl_dir *dir;
	int err;

	dir = lkl_opendir(path, &err);
	if (!dir)
		return err;

	fi->fh = (uintptr_t)dir;

	return 0;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
static int lklfuse_readdir(const char *path, void *buf, fuse_fill_dir_t fill,
			 off_t off, struct fuse_file_info *fi)
{
	struct lkl_dir *dir = (struct lkl_dir *)(uintptr_t)fi->fh;
	struct lkl_linux_dirent64 *de;

	while ((de = lkl_readdir(dir))) {
		struct stat st = { 0, };

		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;

		if (fill(buf, de->d_name, &st, 0))
			break;
	}

	if (!de)
		return lkl_errdir(dir);

	return 0;
}

static int lklfuse_releasedir(const char *path, struct fuse_file_info *fi)
{
	struct lkl_dir *dir = (struct lkl_dir *)(uintptr_t)fi->fh;

	return lkl_closedir(dir);
}

static int lklfuse_fsyncdir(const char *path, int datasync,
			  struct fuse_file_info *fi)
{
	struct lkl_dir *dir = (struct lkl_dir *)(uintptr_t)fi->fh;
	int fd = lkl_dirfd(dir);

	if (datasync)
		return lkl_sys_fdatasync(fd);
	else
		return lkl_sys_fsync(fd);
}

static int lklfuse_access(const char *path, int mode)
{
	return lkl_sys_access(path, mode);
}

static int lklfuse_utimens(const char *path, const struct timespec tv[2])
{
	struct lkl_timespec ts[2];

	ts[0].tv_sec = tv[0].tv_sec;
	ts[0].tv_nsec = tv[0].tv_nsec;
	ts[1].tv_sec = tv[0].tv_sec;
	ts[1].tv_nsec = tv[0].tv_nsec;

	return lkl_sys_utimensat(-1, path, ts, LKL_AT_SYMLINK_NOFOLLOW);
}

static int lklfuse_fallocate(const char *path, int mode, off_t offset,
			     off_t len, struct fuse_file_info *fi)
{
	return lkl_sys_fallocate(fi->fh, mode, offset, len);
}

const struct fuse_operations lklfuse_ops = {
	.flag_nullpath_ok = 1,
	.flag_nopath = 1,
	.flag_utime_omit_ok = 1,

	.getattr = lklfuse_getattr,
	.readlink = lklfuse_readlink,
	.mknod = lklfuse_mknod,
	.mkdir = lklfuse_mkdir,
	.unlink = lklfuse_unlink,
	.rmdir = lklfuse_rmdir,
	.symlink = lklfuse_symlink,
	.rename = lklfuse_rename,
	.link = lklfuse_link,
	.chmod = lklfuse_chmod,
	.chown = lklfuse_chown,
	.truncate = lklfuse_truncate,
	.open = lklfuse_open,
	.read = lklfuse_read,
	.write = lklfuse_write,
	.statfs = lklfuse_statfs,
	.flush = lklfuse_flush,
	.release = lklfuse_release,
	.fsync = lklfuse_fsync,
	.setxattr = lklfuse_setxattr,
	.getxattr = lklfuse_getxattr,
	.listxattr = lklfuse_listxattr,
	.removexattr = lklfuse_removexattr,
	.opendir = lklfuse_opendir,
	.readdir = lklfuse_readdir,
	.releasedir = lklfuse_releasedir,
	.fsyncdir = lklfuse_fsyncdir,
	.access = lklfuse_access,
	.create = lklfuse_create,
	.fgetattr = lklfuse_fgetattr,
	/* .lock, */
	.utimens = lklfuse_utimens,
	/* .bmap, */
	/* .ioctl, */
	/* .poll, */
	/* .write_buf, (SG io) */
	/* .read_buf, (SG io) */
	/* .flock, */
	.fallocate = lklfuse_fallocate,
};

static int start_lkl(void)
{
	long ret;
	char mpoint[32], cmdline[16];

	snprintf(cmdline, sizeof(cmdline), "mem=%dM", lklfuse.mb);
	ret = lkl_start_kernel(&lkl_host_ops, cmdline);
	if (ret) {
		fprintf(stderr, "can't start kernel: %s\n", lkl_strerror(ret));
		goto out;
	}

	ret = lkl_mount_dev(lklfuse.disk_id, lklfuse.part, lklfuse.type,
			    lklfuse.ro ? LKL_MS_RDONLY : 0, lklfuse.opts,
			    mpoint, sizeof(mpoint));

	if (ret) {
		fprintf(stderr, "can't mount disk: %s\n", lkl_strerror(ret));
		goto out_halt;
	}

	ret = lkl_sys_chroot(mpoint);
	if (ret) {
		fprintf(stderr, "can't chdir to %s: %s\n", mpoint,
			lkl_strerror(ret));
		goto out_umount;
	}

	return 0;

out_umount:
	lkl_umount_dev(lklfuse.disk_id, lklfuse.part, 0, 1000);

out_halt:
	lkl_sys_halt();

out:
	return ret;
}

static void stop_lkl(void)
{
	int ret;

	ret = lkl_sys_chdir("/");
	if (ret)
		fprintf(stderr, "can't chdir to /: %s\n", lkl_strerror(ret));
	ret = lkl_sys_umount("/", 0);
	if (ret)
		fprintf(stderr, "failed to umount disk: %d: %s\n",
			lklfuse.disk_id, lkl_strerror(ret));
	lkl_sys_halt();
}

int main(int argc, char **argv)
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *ch;
	struct fuse *fuse;
	struct stat st;
	char *mnt;
	int fg, mt, ret;

	if (fuse_opt_parse(&args, &lklfuse, lklfuse_opts, lklfuse_opt_proc))
		return 1;

	if (!lklfuse.file || !lklfuse.type) {
		fprintf(stderr, "no file or filesystem type specified\n");
		return 1;
	}

	if (fuse_parse_cmdline(&args, &mnt, &mt, &fg))
		return 1;

	ret = stat(mnt, &st);
	if (ret) {
		perror(mnt);
		goto out_free;
	}

	ret = open(lklfuse.file, lklfuse.ro ? O_RDONLY : O_RDWR);
	if (ret < 0) {
		perror(lklfuse.file);
		goto out_free;
	}

	lklfuse.disk.fd = ret;

	ret = lkl_disk_add(&lklfuse.disk);
	if (ret < 0) {
		fprintf(stderr, "can't add disk: %s\n", lkl_strerror(ret));
		goto out_close_disk;
	}

	lklfuse.disk_id = ret;

	ch = fuse_mount(mnt, &args);
	if (!ch) {
		ret = -1;
		goto out_close_disk;
	}

	fuse = fuse_new(ch, &args, &lklfuse_ops, sizeof(lklfuse_ops), NULL);
	if (!fuse) {
		ret = -1;
		goto out_fuse_unmount;
	}

	fuse_opt_free_args(&args);

	if (fuse_daemonize(fg) ||
	    fuse_set_signal_handlers(fuse_get_session(fuse))) {
		ret = -1;
		goto out_fuse_destroy;
	}

	ret = start_lkl();
	if (ret) {
		ret = -1;
		goto out_remove_signals;
	}

	if (mt)
		fprintf(stderr, "warning: multithreaded mode not supported\n");

	ret = fuse_loop(fuse);

	stop_lkl();

out_remove_signals:
	fuse_remove_signal_handlers(fuse_get_session(fuse));

out_fuse_unmount:
	if (ch)
		fuse_unmount(mnt, ch);

out_fuse_destroy:
	if (fuse)
		fuse_destroy(fuse);

out_close_disk:
	close(lklfuse.disk.fd);

out_free:
	free(mnt);

	return ret < 0 ? 1 : 0;
}

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <fuse/fuse_opt.h>
#include <fuse/fuse_lowlevel.h>
#include <lkl.h>
#include <lkl_host.h>

#define LKLFUSE_VERSION "0.1"

struct lklfuse {
	const char *file;
	const char *log;
	const char *type;
	const char *opts;
	struct lkl_disk disk;
	int disk_id;
	int part;
	int ro;
	int mb;
} lklfuse = {
	.mb = 64,
};

#define LKLFUSE_OPT(t, p, v) { t, offsetof(struct lklfuse, p), v }

enum {
	KEY_HELP,
	KEY_VERSION,
};

static struct fuse_opt lklfuse_opts[] = {
	LKLFUSE_OPT("log=%s", log, 0),
	LKLFUSE_OPT("type=%s", type, 0),
	LKLFUSE_OPT("mb=%d", mb, 0),
	LKLFUSE_OPT("opts=%s", opts, 0),
	LKLFUSE_OPT("part=%d", part, 0),
	FUSE_OPT_KEY("-h", KEY_HELP),
	FUSE_OPT_KEY("--help", KEY_HELP),
	FUSE_OPT_KEY("-V",             KEY_VERSION),
	FUSE_OPT_KEY("--version",      KEY_VERSION),
	FUSE_OPT_END
};

static void usage(void)
{
	printf(
"usage: lklfuse file mountpoint [options]\n"
"\n"
"general options:\n"
"    -o opt,[opt...]        mount options\n"
"    -h   --help            print help\n"
"    -V   --version         print version\n"
"\n"
"lklfuse options:\n"
"    -o log=FILE            log file\n"
"    -o type=fstype         filesystem type\n"
"    -o mb=memory in mb     ammount of memory to allocate\n"
"    -o part=parition       partition to mount\n"
"    -o opts=options        mount options (use \\ to escape , and =)\n"
);
}

static int lklfuse_opt_proc(void *data, const char *arg, int key,
			  struct fuse_args *args)
{
	switch (key) {
	case FUSE_OPT_KEY_OPT:
		if (strcmp(arg, "ro") == 0)
			lklfuse.ro = 1;
		return 1;

	case FUSE_OPT_KEY_NONOPT:
		if (!lklfuse.file) {
			lklfuse.file = strdup(arg);
			return 0;
		}
		return 1;

	case KEY_HELP:
		usage();
		fuse_opt_add_arg(args, "-ho");
		fuse_main(args->argc, args->argv, NULL, NULL);
		exit(1);

	case KEY_VERSION:
		printf("lklfuse version %s\n", LKLFUSE_VERSION);
		fuse_opt_add_arg(args, "--version");
		fuse_main(args->argc, args->argv, NULL, NULL);
		exit(0);

	default:
		fprintf(stderr, "internal error\n");
		abort();
	}
}

static void lklfuse_xlat_stat(const struct lkl_stat *in, struct stat *st)
{
	st->st_dev = in->st_dev;
	st->st_ino = in->st_ino;
	st->st_mode = in->st_mode;
	st->st_nlink = in->st_nlink;
	st->st_uid = in->st_uid;
	st->st_gid = in->st_gid;
	st->st_rdev = in->st_rdev;
	st->st_size = in->st_size;
	st->st_blksize = in->st_blksize;
	st->st_blocks = in->st_blocks;
	st->st_atim.tv_sec = in->lkl_st_atime;
	st->st_atim.tv_nsec = in->st_atime_nsec;
	st->st_mtim.tv_sec = in->lkl_st_mtime;
	st->st_mtim.tv_nsec = in->st_mtime_nsec;
	st->st_ctim.tv_sec = in->lkl_st_ctime;
	st->st_ctim.tv_nsec = in->st_ctime_nsec;
}

static int lklfuse_fgetattr(const char *path, struct stat *st,
	                    struct fuse_file_info *fi)
{
	long ret;
	struct lkl_stat lkl_stat;

	ret = lkl_sys_fstat(fi->fh, &lkl_stat);
	if (ret)
		return ret;

	lklfuse_xlat_stat(&lkl_stat, st);
	return 0;
}

static int lklfuse_getattr(const char *path, struct stat *st)
{
	long ret;
	struct lkl_stat lkl_stat;

	ret = lkl_sys_lstat(path, &lkl_stat);
	if (ret)
		return ret;

	lklfuse_xlat_stat(&lkl_stat, st);
	return 0;
}

static int lklfuse_readlink(const char *path, char *buf, size_t len)
{
	long ret;

	ret = lkl_sys_readlink(path, buf, len);
	if (ret < 0)
		return ret;

	if ((size_t)ret == len)
		ret = len - 1;

	buf[ret] = 0;

	return 0;
}

static int lklfuse_mknod(const char *path, mode_t mode, dev_t dev)
{
	return lkl_sys_mknod(path, mode, dev);
}

static int lklfuse_mkdir(const char *path, mode_t mode)
{
	return lkl_sys_mkdir(path, mode);
}

static int lklfuse_unlink(const char *path)
{
	return lkl_sys_unlink(path);
}

static int lklfuse_rmdir(const char *path)
{
	return lkl_sys_rmdir(path);
}

static int lklfuse_symlink(const char *oldname, const char *newname)
{
	return lkl_sys_symlink(oldname, newname);
}


static int lklfuse_rename(const char *oldname, const char *newname)
{
	return lkl_sys_rename(oldname, newname);
}

static int lklfuse_link(const char *oldname, const char *newname)
{
	return lkl_sys_link(oldname, newname);
}

static int lklfuse_chmod(const char *path, mode_t mode)
{
	return lkl_sys_chmod(path, mode);
}


static int lklfuse_chown(const char *path, uid_t uid, gid_t gid)
{
	return lkl_sys_chown(path, uid, gid);
}

static int lklfuse_truncate(const char *path, off_t off)
{
	return lkl_sys_truncate(path, off);
}

static int lklfuse_open3(const char *path, bool create, mode_t mode,
	                 struct fuse_file_info *fi)
{
	long ret;
	int flags;

	if ((fi->flags & O_ACCMODE) == O_RDONLY)
		flags = LKL_O_RDONLY;
	else if ((fi->flags & O_ACCMODE) == O_WRONLY)
		flags = LKL_O_WRONLY;
	else if ((fi->flags & O_ACCMODE) == O_RDWR)
		flags = LKL_O_RDWR;
	else
		return -EINVAL;

	if (create)
		flags |= LKL_O_CREAT;

	ret = lkl_sys_open(path, flags, mode);
	if (ret < 0)
		return ret;

	fi->fh = ret;

	return 0;
}

static int lklfuse_create(const char *path, mode_t mode,
	                  struct fuse_file_info *fi)
{
	return lklfuse_open3(path, true, mode, fi);
}

static int lklfuse_open(const char *path, struct fuse_file_info *fi)
{
	return lklfuse_open3(path, false, 0, fi);
}

static int lklfuse_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	long ret;
	ssize_t orig_size = size;

	do {
		ret = lkl_sys_pread64(fi->fh, buf, size, offset);
		if (ret <= 0)
			break;
		size -= ret;
		offset += ret;
		buf += ret;
	} while (size > 0);

	return ret < 0 ? ret : orig_size - (ssize_t)size;

}

static int lklfuse_write(const char *path, const char *buf, size_t size,
		       off_t offset, struct fuse_file_info *fi)
{
	long ret;
	ssize_t orig_size = size;

	do {
		ret = lkl_sys_pwrite64(fi->fh, buf, size, offset);
		if (ret <= 0)
			break;
		size -= ret;
		offset += ret;
		buf += ret;
	} while (size > 0);

	return ret < 0 ? ret : orig_size - (ssize_t)size;
}


static int lklfuse_statfs(const char *path, struct statvfs *stat)
{
	long ret;
	struct lkl_statfs lkl_statfs;

	ret = lkl_sys_statfs(path, &lkl_statfs);
	if (ret < 0)
		return ret;

	stat->f_bsize = lkl_statfs.f_bsize;
	stat->f_frsize = lkl_statfs.f_frsize;
	stat->f_blocks = lkl_statfs.f_blocks;
	stat->f_bfree = lkl_statfs.f_bfree;
	stat->f_bavail = lkl_statfs.f_bavail;
	stat->f_files = lkl_statfs.f_files;
	stat->f_ffree = lkl_statfs.f_ffree;
	stat->f_favail = stat->f_ffree;
	stat->f_fsid = *(unsigned long *)&lkl_statfs.f_fsid.val[0];
	stat->f_flag = lkl_statfs.f_flags;
	stat->f_namemax = lkl_statfs.f_namelen;

	return 0;
}

static int lklfuse_flush(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int lklfuse_release(const char *path, struct fuse_file_info *fi)
{
	return lkl_sys_close(fi->fh);
}

static int lklfuse_fsync(const char *path, int datasync,
		       struct fuse_file_info *fi)
{
	if (datasync)
		return lkl_sys_fdatasync(fi->fh);
	else
		return lkl_sys_fsync(fi->fh);
}

static int lklfuse_setxattr(const char *path, const char *name, const char *val,
		   size_t size, int flags)
{
	return lkl_sys_setxattr(path, name, val, size, flags);
}

static int lklfuse_getxattr(const char *path, const char *name, char *val,
			  size_t size)
{
	return lkl_sys_getxattr(path, name, val, size);
}

static int lklfuse_listxattr(const char *path, char *list, size_t size)
{
	return lkl_sys_listxattr(path, list, size);
}

static int lklfuse_removexattr(const char *path, const char *name)
{
	return lkl_sys_removexattr(path, name);
}

static int lklfuse_opendir(const char *path, struct fuse_file_info *fi)
{
	struct lkl_dir *dir;
	int err;

	dir = lkl_opendir(path, &err);
	if (!dir)
		return err;

	fi->fh = (uintptr_t)dir;

	return 0;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
static int lklfuse_readdir(const char *path, void *buf, fuse_fill_dir_t fill,
			 off_t off, struct fuse_file_info *fi)
{
	struct lkl_dir *dir = (struct lkl_dir *)(uintptr_t)fi->fh;
	struct lkl_linux_dirent64 *de;

	while ((de = lkl_readdir(dir))) {
		struct stat st = { 0, };

		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;

		if (fill(buf, de->d_name, &st, 0))
			break;
	}

	if (!de)
		return lkl_errdir(dir);

	return 0;
}

static int lklfuse_releasedir(const char *path, struct fuse_file_info *fi)
{
	struct lkl_dir *dir = (struct lkl_dir *)(uintptr_t)fi->fh;

	return lkl_closedir(dir);
}

static int lklfuse_fsyncdir(const char *path, int datasync,
			  struct fuse_file_info *fi)
{
	struct lkl_dir *dir = (struct lkl_dir *)(uintptr_t)fi->fh;
	int fd = lkl_dirfd(dir);

	if (datasync)
		return lkl_sys_fdatasync(fd);
	else
		return lkl_sys_fsync(fd);
}

static int lklfuse_access(const char *path, int mode)
{
	return lkl_sys_access(path, mode);
}

static int lklfuse_utimens(const char *path, const struct timespec tv[2])
{
	struct lkl_timespec ts[2];

	ts[0].tv_sec = tv[0].tv_sec;
	ts[0].tv_nsec = tv[0].tv_nsec;
	ts[1].tv_sec = tv[0].tv_sec;
	ts[1].tv_nsec = tv[0].tv_nsec;

	return lkl_sys_utimensat(-1, path, ts, LKL_AT_SYMLINK_NOFOLLOW);
}

static int lklfuse_fallocate(const char *path, int mode, off_t offset,
			     off_t len, struct fuse_file_info *fi)
{
	return lkl_sys_fallocate(fi->fh, mode, offset, len);
}

const struct fuse_operations lklfuse_ops = {
	.flag_nullpath_ok = 1,
	.flag_nopath = 1,
	.flag_utime_omit_ok = 1,

	.getattr = lklfuse_getattr,
	.readlink = lklfuse_readlink,
	.mknod = lklfuse_mknod,
	.mkdir = lklfuse_mkdir,
	.unlink = lklfuse_unlink,
	.rmdir = lklfuse_rmdir,
	.symlink = lklfuse_symlink,
	.rename = lklfuse_rename,
	.link = lklfuse_link,
	.chmod = lklfuse_chmod,
	.chown = lklfuse_chown,
	.truncate = lklfuse_truncate,
	.open = lklfuse_open,
	.read = lklfuse_read,
	.write = lklfuse_write,
	.statfs = lklfuse_statfs,
	.flush = lklfuse_flush,
	.release = lklfuse_release,
	.fsync = lklfuse_fsync,
	.setxattr = lklfuse_setxattr,
	.getxattr = lklfuse_getxattr,
	.listxattr = lklfuse_listxattr,
	.removexattr = lklfuse_removexattr,
	.opendir = lklfuse_opendir,
	.readdir = lklfuse_readdir,
	.releasedir = lklfuse_releasedir,
	.fsyncdir = lklfuse_fsyncdir,
	.access = lklfuse_access,
	.create = lklfuse_create,
	.fgetattr = lklfuse_fgetattr,
	/* .lock, */
	.utimens = lklfuse_utimens,
	/* .bmap, */
	/* .ioctl, */
	/* .poll, */
	/* .write_buf, (SG io) */
	/* .read_buf, (SG io) */
	/* .flock, */
	.fallocate = lklfuse_fallocate,
};

static int start_lkl(void)
{
	long ret;
	char mpoint[32], cmdline[16];

	snprintf(cmdline, sizeof(cmdline), "mem=%dM", lklfuse.mb);
	ret = lkl_start_kernel(&lkl_host_ops, cmdline);
	if (ret) {
		fprintf(stderr, "can't start kernel: %s\n", lkl_strerror(ret));
		goto out;
	}

	ret = lkl_mount_dev(lklfuse.disk_id, lklfuse.part, lklfuse.type,
			    lklfuse.ro ? LKL_MS_RDONLY : 0, lklfuse.opts,
			    mpoint, sizeof(mpoint));

	if (ret) {
		fprintf(stderr, "can't mount disk: %s\n", lkl_strerror(ret));
		goto out_halt;
	}

	ret = lkl_sys_chroot(mpoint);
	if (ret) {
		fprintf(stderr, "can't chdir to %s: %s\n", mpoint,
			lkl_strerror(ret));
		goto out_umount;
	}

	return 0;

out_umount:
	lkl_umount_dev(lklfuse.disk_id, lklfuse.part, 0, 1000);

out_halt:
	lkl_sys_halt();

out:
	return ret;
}

static void stop_lkl(void)
{
	int ret;

	ret = lkl_sys_chdir("/");
	if (ret)
		fprintf(stderr, "can't chdir to /: %s\n", lkl_strerror(ret));
	ret = lkl_sys_umount("/", 0);
	if (ret)
		fprintf(stderr, "failed to umount disk: %d: %s\n",
			lklfuse.disk_id, lkl_strerror(ret));
	lkl_sys_halt();
}

int main(int argc, char **argv)
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *ch;
	struct fuse *fuse;
	struct stat st;
	char *mnt;
	int fg, mt, ret;

	if (fuse_opt_parse(&args, &lklfuse, lklfuse_opts, lklfuse_opt_proc))
		return 1;

	if (!lklfuse.file || !lklfuse.type) {
		fprintf(stderr, "no file or filesystem type specified\n");
		return 1;
	}

	if (fuse_parse_cmdline(&args, &mnt, &mt, &fg))
		return 1;

	ret = stat(mnt, &st);
	if (ret) {
		perror(mnt);
		goto out_free;
	}

	ret = open(lklfuse.file, lklfuse.ro ? O_RDONLY : O_RDWR);
	if (ret < 0) {
		perror(lklfuse.file);
		goto out_free;
	}

	lklfuse.disk.fd = ret;

	ret = lkl_disk_add(&lklfuse.disk);
	if (ret < 0) {
		fprintf(stderr, "can't add disk: %s\n", lkl_strerror(ret));
		goto out_close_disk;
	}

	lklfuse.disk_id = ret;

	ch = fuse_mount(mnt, &args);
	if (!ch) {
		ret = -1;
		goto out_close_disk;
	}

	fuse = fuse_new(ch, &args, &lklfuse_ops, sizeof(lklfuse_ops), NULL);
	if (!fuse) {
		ret = -1;
		goto out_fuse_unmount;
	}

	fuse_opt_free_args(&args);

	if (fuse_daemonize(fg) ||
	    fuse_set_signal_handlers(fuse_get_session(fuse))) {
		ret = -1;
		goto out_fuse_destroy;
	}

	ret = start_lkl();
	if (ret) {
		ret = -1;
		goto out_remove_signals;
	}

	if (mt)
		fprintf(stderr, "warning: multithreaded mode not supported\n");

	ret = fuse_loop(fuse);

	stop_lkl();

out_remove_signals:
	fuse_remove_signal_handlers(fuse_get_session(fuse));

out_fuse_unmount:
	if (ch)
		fuse_unmount(mnt, ch);

out_fuse_destroy:
	if (fuse)
		fuse_destroy(fuse);

out_close_disk:
	close(lklfuse.disk.fd);

out_free:
	free(mnt);

	return ret < 0 ? 1 : 0;
}

#ifdef __FreeBSD__
#include <sys/param.h>
#endif

#include <stdio.h>
#include <time.h>
#include <argp.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/stat.h>
#ifdef __FreeBSD__
#include <sys/param.h>
#endif

#include <stdio.h>
#include <time.h>
#include <argp.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include <lkl.h>
#include <lkl_host.h>

char doc[] = "";
char args_doc[] = "-t fstype fsimage_path tar_path";
static struct argp_option options[] = {
	{"enable-printk", 'p', 0, 0, "show Linux printks"},
	{"partition", 'P', "int", 0, "partition number"},
	{"filesystem-type", 't', "string", 0,
	 "select filesystem type - mandatory"},
	{"selinux-contexts", 's', "file", 0,
	 "export selinux contexts to file"},
	{0},
};

static struct cl_args {
	int printk;
	int part;
	const char *fsimg_type;
	const char *fsimg_path;
	const char *tar_path;
	FILE *selinux;
} cla;

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct cl_args *cla = state->input;

	switch (key) {
	case 'p':
		cla->printk = 1;
		break;
	case 'P':
		cla->part = atoi(arg);
		break;
	case 't':
		cla->fsimg_type = arg;
		break;
	case 's':
		cla->selinux = fopen(arg, "w");
		if (!cla->selinux) {
			fprintf(stderr, "failed to open selinux contexts file: %s\n",
				strerror(errno));
			return -1;
		}
		break;
	case ARGP_KEY_ARG:
		if (!cla->fsimg_path)
			cla->fsimg_path = arg;
		else if (!cla->tar_path)
			cla->tar_path = arg;
		else
			return -1;
		break;
	case ARGP_KEY_END:
		if (state->arg_num < 2 || !cla->fsimg_type)
			argp_usage(state);
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

static struct archive *tar;

static int searchdir(const char *fsimg_path, const char *path);

static int copy_file(const char *fsimg_path, const char *path)
{
	long fsimg_fd;
	char buff[4096];
	long len, wrote;
	int ret = 0;

	fsimg_fd = lkl_sys_open(fsimg_path, LKL_O_RDONLY, 0);
	if (fsimg_fd < 0) {
		fprintf(stderr, "fsimg error opening %s: %s\n", fsimg_path,
			lkl_strerror(fsimg_fd));
		return fsimg_fd;
	}

	do {
		len = lkl_sys_read(fsimg_fd, buff, sizeof(buff));
		if (len > 0) {
			wrote = archive_write_data(tar, buff, len);
			if (wrote != len) {
				fprintf(stderr, "error writing file %s to archive: %s [%d %ld]\n",
					path, archive_error_string(tar), ret,
					len);
				ret = -archive_errno(tar);
				break;
			}
		}

		if (len < 0) {
			fprintf(stderr, "error reading fsimg file %s: %s\n",
				fsimg_path, lkl_strerror(len));
			ret = len;
		}

	} while (len > 0);

	lkl_sys_close(fsimg_fd);

	return ret;
}

static int add_link(const char *fsimg_path, const char *path,
		    struct archive_entry *entry)
{
	char buf[4096] = { 0, };
	long len;

	len = lkl_sys_readlink(fsimg_path, buf, sizeof(buf));
	if (len < 0) {
		fprintf(stderr, "fsimg readlink error %s: %s\n",
			fsimg_path, lkl_strerror(len));
		return len;
	}

	archive_entry_set_symlink(entry, buf);

	return 0;
}

static inline void fsimg_copy_stat(struct stat *st, struct lkl_stat *fst)
{
	st->st_dev = fst->st_dev;
	st->st_ino = fst->st_ino;
	st->st_mode = fst->st_mode;
	st->st_nlink = fst->st_nlink;
	st->st_uid = fst->st_uid;
	st->st_gid = fst->st_gid;
	st->st_rdev = fst->st_rdev;
	st->st_size = fst->st_size;
	st->st_blksize = fst->st_blksize;
	st->st_blocks = fst->st_blocks;
	st->st_atim.tv_sec = fst->lkl_st_atime;
	st->st_atim.tv_nsec = fst->st_atime_nsec;
	st->st_mtim.tv_sec = fst->lkl_st_mtime;
	st->st_mtim.tv_nsec = fst->st_mtime_nsec;
	st->st_ctim.tv_sec = fst->lkl_st_ctime;
	st->st_ctim.tv_nsec = fst->st_ctime_nsec;
}

static int copy_xattr(const char *fsimg_path, const char *path,
		      struct archive_entry *entry)
{
	long ret;
	char *xattr_list, *i;
	long xattr_list_size;

	ret = lkl_sys_llistxattr(fsimg_path, NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "fsimg llistxattr(%s) error: %s\n",
			path, lkl_strerror(ret));
		return ret;
	}

	if (!ret)
		return 0;

	xattr_list = malloc(ret);

	ret = lkl_sys_llistxattr(fsimg_path, xattr_list, ret);
	if (ret < 0) {
		fprintf(stderr, "fsimg llistxattr(%s) error: %s\n", path,
			lkl_strerror(ret));
		free(xattr_list);
		return ret;
	}

	xattr_list_size = ret;

	for (i = xattr_list; i - xattr_list < xattr_list_size;
	     i += strlen(i) + 1) {
		void *xattr_buf;

		ret = lkl_sys_lgetxattr(fsimg_path, i, NULL, 0);
		if (ret < 0) {
			fprintf(stderr, "fsimg lgetxattr(%s) error: %s\n", path,
				lkl_strerror(ret));
			free(xattr_list);
			return ret;
		}

		xattr_buf = malloc(ret);

		ret = lkl_sys_lgetxattr(fsimg_path, i, xattr_buf, ret);
		if (ret < 0) {
			fprintf(stderr, "fsimg lgetxattr2(%s) error: %s\n",
				path, lkl_strerror(ret));
			free(xattr_list);
			free(xattr_buf);
			return ret;
		}

		if (cla.selinux && strcmp(i, "security.selinux") == 0)
			fprintf(cla.selinux, "%s %s\n", path,
				(char *)xattr_buf);

		archive_entry_xattr_clear(entry);
		archive_entry_xattr_add_entry(entry, i, xattr_buf, ret);

		free(xattr_buf);
	}

	free(xattr_list);

	return 0;
}

static int do_entry(const char *fsimg_path, const char *path,
		    const struct lkl_linux_dirent64 *de)
{
	char fsimg_new_path[PATH_MAX], new_path[PATH_MAX];
	struct lkl_stat fsimg_stat;
	struct stat stat;
	struct archive_entry *entry;
	int ftype;
	long ret;

	snprintf(new_path, sizeof(new_path), "%s/%s", path, de->d_name);
	snprintf(fsimg_new_path, sizeof(fsimg_new_path), "%s/%s", fsimg_path,
		 de->d_name);

	ret = lkl_sys_lstat(fsimg_new_path, &fsimg_stat);
	if (ret) {
		fprintf(stderr, "fsimg lstat(%s) error: %s\n",
			path, lkl_strerror(ret));
		return ret;
	}

	entry = archive_entry_new();

	archive_entry_set_pathname(entry, new_path);
	fsimg_copy_stat(&stat, &fsimg_stat);
	archive_entry_copy_stat(entry, &stat);
	ret = copy_xattr(fsimg_new_path, new_path, entry);
	if (ret)
		return ret;
	/* TODO: ACLs */

	ftype = stat.st_mode & S_IFMT;

	switch (ftype) {
	case S_IFREG:
		archive_write_header(tar, entry);
		ret = copy_file(fsimg_new_path, new_path);
		break;
	case S_IFDIR:
		archive_write_header(tar, entry);
		ret = searchdir(fsimg_new_path, new_path);
		break;
	case S_IFLNK:
		ret = add_link(fsimg_new_path, new_path, entry);
		/* fall through */
	case S_IFSOCK:
	case S_IFBLK:
	case S_IFCHR:
	case S_IFIFO:
		if (ret)
			break;
		archive_write_header(tar, entry);
		break;
	default:
		printf("skipping %s: unsupported entry type %d\n", new_path,
		       ftype);
	}

	archive_entry_free(entry);

	if (ret)
		printf("error processing entry %s, aborting\n", new_path);

	return ret;
}

static int searchdir(const char *fsimg_path, const char *path)
{
	long ret, fd;
	char buf[1024], *pos;
	long buf_len;

	fd = lkl_sys_open(fsimg_path, LKL_O_RDONLY | LKL_O_DIRECTORY, 0);
	if (fd < 0) {
		fprintf(stderr, "failed to open dir %s: %s", fsimg_path,
			lkl_strerror(fd));
		return fd;
	}

	do {
		struct lkl_linux_dirent64 *de;

		de = (struct lkl_linux_dirent64 *) buf;
		buf_len = lkl_sys_getdents64(fd, de, sizeof(buf));
		if (buf_len < 0) {
			fprintf(stderr, "gentdents64 error: %s\n",
				lkl_strerror(buf_len));
			break;
		}

		for (pos = buf; pos - buf < buf_len; pos += de->d_reclen) {
			de = (struct lkl_linux_dirent64 *)pos;

			if (!strcmp(de->d_name, ".") ||
			    !strcmp(de->d_name, ".."))
				continue;

			ret = do_entry(fsimg_path, path, de);
			if (ret)
				goto out;
		}

	} while (buf_len > 0);

out:
	lkl_sys_close(fd);
	return ret;
}

int main(int argc, char **argv)
{
	struct lkl_disk disk;
	long ret;
	char mpoint[32];
	unsigned int disk_id;

	if (argp_parse(&argp, argc, argv, 0, 0, &cla) < 0)
		return -1;

	if (!cla.printk)
		lkl_host_ops.print = NULL;

	disk.fd = open(cla.fsimg_path, O_RDONLY);
	if (disk.fd < 0) {
		fprintf(stderr, "can't open fsimg %s: %s\n", cla.fsimg_path,
			strerror(errno));
		ret = 1;
		goto out;
	}

	disk.ops = NULL;

	ret = lkl_disk_add(&disk);
	if (ret < 0) {
		fprintf(stderr, "can't add disk: %s\n", lkl_strerror(ret));
		goto out_close;
	}
	disk_id = ret;

	lkl_start_kernel(&lkl_host_ops, "mem=10M");

	ret = lkl_mount_dev(disk_id, cla.part, cla.fsimg_type, LKL_MS_RDONLY,
			    NULL, mpoint, sizeof(mpoint));
	if (ret) {
		fprintf(stderr, "can't mount disk: %s\n", lkl_strerror(ret));
		goto out_close;
	}

	ret = lkl_sys_chdir(mpoint);
	if (ret) {
		fprintf(stderr, "can't chdir to %s: %s\n", mpoint,
			lkl_strerror(ret));
		goto out_umount;
	}

	tar = archive_write_new();
	archive_write_set_format_pax_restricted(tar);
	archive_write_open_filename(tar, cla.tar_path);

	ret = searchdir(mpoint, "");

	archive_write_free(tar);

	if (cla.selinux)
		fclose(cla.selinux);

out_umount:
	lkl_umount_dev(disk_id, cla.part, 0, 1000);

out_close:
	close(disk.fd);

out:
	lkl_sys_halt();

	return ret;
}

#include <fcntl.h>
#include <archive.h>
#include <archive_entry.h>
#include <lkl.h>
#include <lkl_host.h>

char doc[] = "";
char args_doc[] = "-t fstype fsimage_path tar_path";
static struct argp_option options[] = {
	{"enable-printk", 'p', 0, 0, "show Linux printks"},
	{"partition", 'P', "int", 0, "partition number"},
	{"filesystem-type", 't', "string", 0,
	 "select filesystem type - mandatory"},
	{"selinux-contexts", 's', "file", 0,
	 "export selinux contexts to file"},
	{0},
};

static struct cl_args {
	int printk;
	int part;
	const char *fsimg_type;
	const char *fsimg_path;
	const char *tar_path;
	FILE *selinux;
} cla;

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct cl_args *cla = state->input;

	switch (key) {
	case 'p':
		cla->printk = 1;
		break;
	case 'P':
		cla->part = atoi(arg);
		break;
	case 't':
		cla->fsimg_type = arg;
		break;
	case 's':
		cla->selinux = fopen(arg, "w");
		if (!cla->selinux) {
			fprintf(stderr, "failed to open selinux contexts file: %s\n",
				strerror(errno));
			return -1;
		}
		break;
	case ARGP_KEY_ARG:
		if (!cla->fsimg_path)
			cla->fsimg_path = arg;
		else if (!cla->tar_path)
			cla->tar_path = arg;
		else
			return -1;
		break;
	case ARGP_KEY_END:
		if (state->arg_num < 2 || !cla->fsimg_type)
			argp_usage(state);
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

static struct archive *tar;

static int searchdir(const char *fsimg_path, const char *path);

static int copy_file(const char *fsimg_path, const char *path)
{
	long fsimg_fd;
	char buff[4096];
	long len, wrote;
	int ret = 0;

	fsimg_fd = lkl_sys_open(fsimg_path, LKL_O_RDONLY, 0);
	if (fsimg_fd < 0) {
		fprintf(stderr, "fsimg error opening %s: %s\n", fsimg_path,
			lkl_strerror(fsimg_fd));
		return fsimg_fd;
	}

	do {
		len = lkl_sys_read(fsimg_fd, buff, sizeof(buff));
		if (len > 0) {
			wrote = archive_write_data(tar, buff, len);
			if (wrote != len) {
				fprintf(stderr, "error writing file %s to archive: %s [%d %ld]\n",
					path, archive_error_string(tar), ret,
					len);
				ret = -archive_errno(tar);
				break;
			}
		}

		if (len < 0) {
			fprintf(stderr, "error reading fsimg file %s: %s\n",
				fsimg_path, lkl_strerror(len));
			ret = len;
		}

	} while (len > 0);

	lkl_sys_close(fsimg_fd);

	return ret;
}

static int add_link(const char *fsimg_path, const char *path,
		    struct archive_entry *entry)
{
	char buf[4096] = { 0, };
	long len;

	len = lkl_sys_readlink(fsimg_path, buf, sizeof(buf));
	if (len < 0) {
		fprintf(stderr, "fsimg readlink error %s: %s\n",
			fsimg_path, lkl_strerror(len));
		return len;
	}

	archive_entry_set_symlink(entry, buf);

	return 0;
}

static inline void fsimg_copy_stat(struct stat *st, struct lkl_stat *fst)
{
	st->st_dev = fst->st_dev;
	st->st_ino = fst->st_ino;
	st->st_mode = fst->st_mode;
	st->st_nlink = fst->st_nlink;
	st->st_uid = fst->st_uid;
	st->st_gid = fst->st_gid;
	st->st_rdev = fst->st_rdev;
	st->st_size = fst->st_size;
	st->st_blksize = fst->st_blksize;
	st->st_blocks = fst->st_blocks;
	st->st_atim.tv_sec = fst->lkl_st_atime;
	st->st_atim.tv_nsec = fst->st_atime_nsec;
	st->st_mtim.tv_sec = fst->lkl_st_mtime;
	st->st_mtim.tv_nsec = fst->st_mtime_nsec;
	st->st_ctim.tv_sec = fst->lkl_st_ctime;
	st->st_ctim.tv_nsec = fst->st_ctime_nsec;
}

static int copy_xattr(const char *fsimg_path, const char *path,
		      struct archive_entry *entry)
{
	long ret;
	char *xattr_list, *i;
	long xattr_list_size;

	ret = lkl_sys_llistxattr(fsimg_path, NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "fsimg llistxattr(%s) error: %s\n",
			path, lkl_strerror(ret));
		return ret;
	}

	if (!ret)
		return 0;

	xattr_list = malloc(ret);

	ret = lkl_sys_llistxattr(fsimg_path, xattr_list, ret);
	if (ret < 0) {
		fprintf(stderr, "fsimg llistxattr(%s) error: %s\n", path,
			lkl_strerror(ret));
		free(xattr_list);
		return ret;
	}

	xattr_list_size = ret;

	for (i = xattr_list; i - xattr_list < xattr_list_size;
	     i += strlen(i) + 1) {
		void *xattr_buf;

		ret = lkl_sys_lgetxattr(fsimg_path, i, NULL, 0);
		if (ret < 0) {
			fprintf(stderr, "fsimg lgetxattr(%s) error: %s\n", path,
				lkl_strerror(ret));
			free(xattr_list);
			return ret;
		}

		xattr_buf = malloc(ret);

		ret = lkl_sys_lgetxattr(fsimg_path, i, xattr_buf, ret);
		if (ret < 0) {
			fprintf(stderr, "fsimg lgetxattr2(%s) error: %s\n",
				path, lkl_strerror(ret));
			free(xattr_list);
			free(xattr_buf);
			return ret;
		}

		if (cla.selinux && strcmp(i, "security.selinux") == 0)
			fprintf(cla.selinux, "%s %s\n", path,
				(char *)xattr_buf);

		archive_entry_xattr_clear(entry);
		archive_entry_xattr_add_entry(entry, i, xattr_buf, ret);

		free(xattr_buf);
	}

	free(xattr_list);

	return 0;
}

static int do_entry(const char *fsimg_path, const char *path,
		    const struct lkl_linux_dirent64 *de)
{
	char fsimg_new_path[PATH_MAX], new_path[PATH_MAX];
	struct lkl_stat fsimg_stat;
	struct stat stat;
	struct archive_entry *entry;
	int ftype;
	long ret;

	snprintf(new_path, sizeof(new_path), "%s/%s", path, de->d_name);
	snprintf(fsimg_new_path, sizeof(fsimg_new_path), "%s/%s", fsimg_path,
		 de->d_name);

	ret = lkl_sys_lstat(fsimg_new_path, &fsimg_stat);
	if (ret) {
		fprintf(stderr, "fsimg lstat(%s) error: %s\n",
			path, lkl_strerror(ret));
		return ret;
	}

	entry = archive_entry_new();

	archive_entry_set_pathname(entry, new_path);
	fsimg_copy_stat(&stat, &fsimg_stat);
	archive_entry_copy_stat(entry, &stat);
	ret = copy_xattr(fsimg_new_path, new_path, entry);
	if (ret)
		return ret;
	/* TODO: ACLs */

	ftype = stat.st_mode & S_IFMT;

	switch (ftype) {
	case S_IFREG:
		archive_write_header(tar, entry);
		ret = copy_file(fsimg_new_path, new_path);
		break;
	case S_IFDIR:
		archive_write_header(tar, entry);
		ret = searchdir(fsimg_new_path, new_path);
		break;
	case S_IFLNK:
		ret = add_link(fsimg_new_path, new_path, entry);
		/* fall through */
	case S_IFSOCK:
	case S_IFBLK:
	case S_IFCHR:
	case S_IFIFO:
		if (ret)
			break;
		archive_write_header(tar, entry);
		break;
	default:
		printf("skipping %s: unsupported entry type %d\n", new_path,
		       ftype);
	}

	archive_entry_free(entry);

	if (ret)
		printf("error processing entry %s, aborting\n", new_path);

	return ret;
}

static int searchdir(const char *fsimg_path, const char *path)
{
	long ret, fd;
	char buf[1024], *pos;
	long buf_len;

	fd = lkl_sys_open(fsimg_path, LKL_O_RDONLY | LKL_O_DIRECTORY, 0);
	if (fd < 0) {
		fprintf(stderr, "failed to open dir %s: %s", fsimg_path,
			lkl_strerror(fd));
		return fd;
	}

	do {
		struct lkl_linux_dirent64 *de;

		de = (struct lkl_linux_dirent64 *) buf;
		buf_len = lkl_sys_getdents64(fd, de, sizeof(buf));
		if (buf_len < 0) {
			fprintf(stderr, "gentdents64 error: %s\n",
				lkl_strerror(buf_len));
			break;
		}

		for (pos = buf; pos - buf < buf_len; pos += de->d_reclen) {
			de = (struct lkl_linux_dirent64 *)pos;

			if (!strcmp(de->d_name, ".") ||
			    !strcmp(de->d_name, ".."))
				continue;

			ret = do_entry(fsimg_path, path, de);
			if (ret)
				goto out;
		}

	} while (buf_len > 0);

out:
	lkl_sys_close(fd);
	return ret;
}

int main(int argc, char **argv)
{
	struct lkl_disk disk;
	long ret;
	char mpoint[32];
	unsigned int disk_id;

	if (argp_parse(&argp, argc, argv, 0, 0, &cla) < 0)
		return -1;

	if (!cla.printk)
		lkl_host_ops.print = NULL;

	disk.fd = open(cla.fsimg_path, O_RDONLY);
	if (disk.fd < 0) {
		fprintf(stderr, "can't open fsimg %s: %s\n", cla.fsimg_path,
			strerror(errno));
		ret = 1;
		goto out;
	}

	disk.ops = NULL;

	ret = lkl_disk_add(&disk);
	if (ret < 0) {
		fprintf(stderr, "can't add disk: %s\n", lkl_strerror(ret));
		goto out_close;
	}
	disk_id = ret;

	lkl_start_kernel(&lkl_host_ops, "mem=10M");

	ret = lkl_mount_dev(disk_id, cla.part, cla.fsimg_type, LKL_MS_RDONLY,
			    NULL, mpoint, sizeof(mpoint));
	if (ret) {
		fprintf(stderr, "can't mount disk: %s\n", lkl_strerror(ret));
		goto out_close;
	}

	ret = lkl_sys_chdir(mpoint);
	if (ret) {
		fprintf(stderr, "can't chdir to %s: %s\n", mpoint,
			lkl_strerror(ret));
		goto out_umount;
	}

	tar = archive_write_new();
	archive_write_set_format_pax_restricted(tar);
	archive_write_open_filename(tar, cla.tar_path);

	ret = searchdir(mpoint, "");

	archive_write_free(tar);

	if (cla.selinux)
		fclose(cla.selinux);

out_umount:
	lkl_umount_dev(disk_id, cla.part, 0, 1000);

out_close:
	close(disk.fd);

out:
	lkl_sys_halt();

	return ret;
}
