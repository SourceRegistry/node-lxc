/**
 * @author A.P.A. Slaa (a.p.a.slaa@projectsource.nl) ProjectSource V.O.F.
 * @date 03-07-2024
 */

import {Personality} from "./Personality"
import {lxc_attach_env_policy} from "./lxc_attach_env_policy";
import {LXC_ATTACH} from "./LXC_ATTACH";


export type lxc_attach_options = ({
    /**
     * If ClearEnv is true, the environment is cleared before running the command.
     */
    env_policy: lxc_attach_env_policy.LXC_ATTACH_CLEAR_ENV,
    /**
     * EnvToKeep specifies the environment of the process when ClearEnv is true.
     */
    extra_keep_env: string[]
} |
    {
        /**
         *  Retain the environment
         */
        env_policy: lxc_attach_env_policy.LXC_ATTACH_KEEP_ENV,
    }) &
    {
        /**
         * Any combination of LXC_ATTACH_* flags
         * @link [https://linuxcontainers.org/apidocs](https://linuxcontainers.org/lxc/apidoc/structlxc__attach__options__t.html#a14f47c7faff4c413e05d4b0fa3e2fa6f)
         */
        attach_flags: number,
        /**
         * Specify the namespaces to attach to, as OR'ed list of clone flags (syscall.CLONE_NEWNS | syscall.CLONE_NEWUTS ...).
         * @link [https://linuxcontainers.org/apidocs](https://linuxcontainers.org/lxc/apidoc/structlxc__attach__options__t.html#a80a7d14f141d44e92cab818a473a51e4)
         */
        namespaces: number,
        /**
         * Specify the architecture which the kernel should appear to be running as to the command executed.
         * Initial personality (LXC_ATTACH_DETECT_PERSONALITY to autodetect).
         * @warning This may be ignored if lxc is compiled without personality support)
         * @link [https://linuxcontainers.org/apidocs](https://linuxcontainers.org/lxc/apidoc/structlxc__attach__options__t.html#a22232139102062bcf18e1f7ae6aea17d)
         */
        personality: Personality | -1 | bigint,
        /**
         * If the current directory does not exist in the container, the root directory will be used instead because of kernel defaults.
         * @link [https://linuxcontainers.org/apidocs](https://linuxcontainers.org/lxc/apidoc/structlxc__attach__options__t.html#ab6a000a4ac27f2ec3d861c55499eaff2)
         */
        initial_cwd?: string | undefined,
        /**
         * UID specifies the user id to run as.
         * @note Set to -1 for default behaviour (init uid for userns containers or 0 (super-user) if detection fails).
         * @link [https://linuxcontainers.org/apidocs](https://linuxcontainers.org/lxc/apidoc/structlxc__attach__options__t.html#a8a62e9318622bc396fdd24ab1a89bf60)
         */
        uid?: number,
        /**
         * GID specifies the group id to run as.
         * @link [https://linuxcontainers.org/apidocs](https://linuxcontainers.org/lxc/apidoc/structlxc__attach__options__t.html#a7e54d34f03a8a366ff3a64306fd61dfe)
         * @note Set to -1 for default behaviour (init gid for userns containers or 0 (super-user) if detection fails).
         */
        gid?: number,
        /**
         * Groups specifies the list of additional group ids to run with.
         * The additional group GIDs to run with.
         * If unset all additional groups are dropped.
         * @link [](https://linuxcontainers.org/lxc/apidoc/structlxc__attach__options__t.html#af71a28922efb5b4003c20cec759b68fd)
         */
        groups?: number[],
        /**
         * [stdinfd, stdoutfd, stderrfd]
         * stdinfd: specifies the fd to read input from.
         * stdoutdf: specifies the fd to write output to.
         * stderrfd: specifies the fd to write error output to.
         */
        stdio: [number, number, number],

        /**
         * Env specifies the environment of the process.
         */
        extra_env_vars: string[],
        /**
         * RemountSysProc remounts /sys and /proc for the executed command.
         * This is required to reflect the container (PID) namespace context
         * if the command does not attach to the container's mount namespace.
         */
        remount_sys_proc: boolean
        /**
         * ElevatedPrivileges runs the command with elevated privileges.
         * The capabilities, cgroup and security module restrictions of the container are not applied.
         * @WARNING: This may leak privileges into the container.
         */
        elevated_privileges: boolean,
        /**
         * File descriptor to log output.
         */
        log_fd: number,
        /**
         * lsm label to set.
         */
        lsm_label: string
    }


/**
 * Default attach options to use
 * @link [https://github.com/lxc/lxc](https://github.com/lxc/lxc/blob/main/src/lxc/attach_options.h#L161C9-L161C40)
 */
export const DEFAULT_ATTACH: lxc_attach_options = {
    attach_flags: LXC_ATTACH.DEFAULT,
    namespaces: -1,
    personality: -1,
    initial_cwd: "/",
    uid: -1,
    gid: -1,
    groups: [],
    env_policy: lxc_attach_env_policy.LXC_ATTACH_KEEP_ENV,
    extra_env_vars: [],
    stdio: [process.stdin.fd, process.stdout.fd, process.stderr.fd],
    remount_sys_proc: false,
    elevated_privileges: false,
    log_fd: -1,
    lsm_label: undefined
}
