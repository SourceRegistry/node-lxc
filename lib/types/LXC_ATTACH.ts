/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 03-07-2024
 */

export enum LXC_ATTACH {
    /**
     * Move to cgroup
     * @default on
     */
    MOVE_TO_CGROUP = 0x00000001,
    /**
     * Drop capabilities
     * @default on
     */
    DROP_CAPABILITIES = 0x00000002,

    /**
     * Set personality
     * @default on
     */
    SET_PERSONALITY = 0x00000004,

    /**
     * Execute under a Linux Security Module
     * @default on
     */
    LSM_EXEC = 0x00000008,

    /**
     * Remount /proc filesystem
     * @default off
     */
    REMOUNT_PROC_SYS = 0x00010000,

    /**
     * TODO: currently unused
     * @default off
     */
    LSM_NOW = 0x00020000,

    /**
     * PR_SET_NO_NEW_PRIVS
     * Set PR_SET_NO_NEW_PRIVS to block execve() gainable privileges.
     *
     * @default off
     *
     */
    NO_NEW_PRIVS = 0x00040000,

    /**
     * Allocate new terminal for attached process.
     * @default off
     */
    TERMINAL = 0x00080000,

    /**
     * Set custom LSM label specified in @lsm_label.
     * @default off
     */
    LSM_LABEL = 0x00100000,

    /**
     * Set additional group ids specified in @groups.
     * @default off
     */
    SETGROUPS = 0x00200000,

    /**
     * We have 16 bits for things that are on by default and 16 bits that
     * are off by default, that should be sufficient to keep binary
     * compatibility for a while
     *
     * Mask of flags to apply by default
     */
    DEFAULT = 0x0000FFFF
}
