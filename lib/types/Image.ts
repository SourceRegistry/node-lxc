export type Image = {
    readonly aliases: string,
    readonly arch: string,
    readonly os: string,
    readonly release: string,
    readonly release_title: string,
    readonly requirements: Record<string, string>
    readonly variant: string,

    readonly versions: Record<string, {
        files: Record<string, {
            ftype: string,
            sha256: string,
            size: number,
            path: string,
            combined_sha256?: string,
            combined_rootxz_sha256?: string,
            combined_squashfs_sha256?: string
            "combined_disk-kvm-img_sha256?": string
            delta_base?: string
        }>
    }>

}
