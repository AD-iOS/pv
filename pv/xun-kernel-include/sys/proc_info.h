/* The simplified version of the proc_info.h header document used in iOS compilation */

#ifndef _SYS_PROC_INFO_H
#define _SYS_PROC_INFO_H

#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>

#define PROC_PIDLISTFDS                 1
#define PROC_PIDFDVNODEPATHINFO         2
#define PROX_FDTYPE_VNODE               1

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#ifndef MAXCOMLEN
#define MAXCOMLEN 16
#endif

#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif

__BEGIN_DECLS

struct proc_fdinfo {
    int32_t     proc_fd;
    uint32_t    proc_fdtype;
};

struct proc_fileinfo {
    uint32_t    fi_openflags;
    uint32_t    fi_status;
    off_t       fi_offset;
    int32_t     fi_type;
    uint32_t    fi_guardflags;
};

struct vinfo_stat {
    uint32_t    vst_dev;
    uint16_t    vst_mode;
    uint16_t    vst_nlink;
    uint64_t    vst_ino;
    uid_t       vst_uid;
    gid_t       vst_gid;

    int64_t     vst_atime;
    int64_t     vst_atimensec;
    int64_t     vst_mtime;
    int64_t     vst_mtimensec;
    int64_t     vst_ctime;
    int64_t     vst_ctimensec;
    int64_t     vst_birthtime;
    int64_t     vst_birthtimensec;
    off_t       vst_size;
    int64_t     vst_blocks;
    int32_t     vst_blksize;
    uint32_t    vst_flags;
    uint32_t    vst_gen;
    uint32_t    vst_rdev;
    int64_t     vst_qspare[2];
};

struct vnode_info_path {
    struct vinfo_stat       vip_vi;
    char                    vip_path[MAXPATHLEN];
};

struct vnode_fdinfowithpath {
    struct proc_fileinfo    pfi;
    struct vnode_info_path  pvip;
};

#define PROC_PIDFDVNODEPATHINFO_SIZE    sizeof(struct vnode_fdinfowithpath)
#define PROC_PIDLISTFD_SIZE             sizeof(struct proc_fdinfo)

__END_DECLS

#endif /* _SYS_PROC_INFO_H */