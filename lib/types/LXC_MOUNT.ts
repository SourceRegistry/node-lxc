/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 03-07-2024
 */

export enum LXC_MOUNT_VERSION{
    V1 = 0
}

export type lxc_mount = {
    /**
     * only value LXC_MOUNT_API_V1 = 1
     */
    version: LXC_MOUNT_VERSION
}

/* These are the fs-independent mount-flags: up to 16 flags are
   supported  */
export enum LXC_MOUNT {
    MS_RDONLY = 1,		/* Mount read-only.  */
    MS_NOSUID = 2,		/* Ignore suid and sgid bits.  */
    MS_NODEV = 4,			/* Disallow access to device special files.  */
    MS_NOEXEC = 8,		/* Disallow program execution.  */
    MS_SYNCHRONOUS = 16,		/* Writes are synced at once.  */
    MS_REMOUNT = 32,		/* Alter flags of a mounted FS.  */
    MS_MANDLOCK = 64,		/* Allow mandatory locks on an FS.  */
    MS_DIRSYNC = 128,		/* Directory modifications are synchronous.  */
    MS_NOSYMFOLLOW = 256,		/* Do not follow symlinks.  */
    MS_NOATIME = 1024,		/* Do not update access times.  */
    MS_NODIRATIME = 2048,		/* Do not update directory access times.  */
    MS_BIND = 4096,		/* Bind directory at different place.  */
    MS_MOVE = 8192,
    MS_REC = 16384,
    MS_SILENT = 32768,
    MS_POSIXACL = 1 << 16,	/* VFS does not apply the umask.  */
    MS_UNBINDABLE = 1 << 17,	/* Change to unbindable.  */
    MS_PRIVATE = 1 << 18,		/* Change to private.  */
    MS_SLAVE = 1 << 19,		/* Change to slave.  */
    MS_SHARED = 1 << 20,		/* Change to shared.  */
    MS_RELATIME = 1 << 21,	/* Update atime relative to mtime/ctime.  */
    MS_KERNMOUNT = 1 << 22,	/* This is a kern_mount call.  */
    MS_I_VERSION = 1 << 23,	/* Update inode I_version field.  */
    MS_STRICTATIME = 1 << 24,	/* Always perform atime updates.  */
    MS_LAZYTIME = 1 << 25,	/* Update the on-disk [acm]times lazily.  */
    MS_ACTIVE = 1 << 30,
    MS_NOUSER = 1 << 31
};
