# bash completion for dwarfs-fm
_dwarfs_fm() {
    local cur prev cmds
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD - 1]}"
    cmds="open archive unarchive check convert mount unmount"

    if [[ ${COMP_CWORD} -eq 1 ]]; then
        COMPREPLY=($(compgen -W "${cmds} -h --help --version -v -q" -- "${cur}"))
        return 0
    fi

    case "${COMP_WORDS[1]}" in
    open | unarchive | check | convert | mount | unmount | archive)
        COMPREPLY=($(compgen -f -- "${cur}"))
        ;;
    esac
}
complete -F _dwarfs_fm dwarfs-fm
