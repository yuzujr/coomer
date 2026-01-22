# coomer fish completions

function __coomer_backend_from_cmdline
    set -l words (commandline -opc)
    for i in (seq 1 (count $words))
        if test $words[$i] = --backend
            set -l j (math $i + 1)
            if test $j -le (count $words)
                echo $words[$j]
                return 0
            end
        end
    end
    return 1
end

function __coomer_list_monitors
    set -l backend (__coomer_backend_from_cmdline)
    if test -n "$backend"
        coomer --backend $backend --list-monitors 2>/dev/null \
            | string match -r '^\[[0-9]+\]\s+(\S+)' \
            | string replace -r '^\[[0-9]+\]\s+(\S+).*' '$1'
    else
        coomer --list-monitors 2>/dev/null \
            | string match -r '^\[[0-9]+\]\s+(\S+)' \
            | string replace -r '^\[[0-9]+\]\s+(\S+).*' '$1'
    end
end

# --backend <mode>: auto|x11|wlr|portal
complete -c coomer -l backend -r -f \
    -a "auto x11 wlr portal" \
    -d "Capture backend (default: auto)"

# --monitor <name>
complete -c coomer -l monitor -r -f \
    -a "(__coomer_list_monitors)" \
    -d "Select monitor/output by name (x11/wlr only)"

# --list-monitors
complete -c coomer -l list-monitors \
    -d "List monitors/outputs visible to the backend (x11/wlr only)"

# --overlay
complete -c coomer -l overlay \
    -d "Wayland layer-shell overlay (wlr/portal only)"

# --portal-interactive
complete -c coomer -l portal-interactive \
    -d "Enable interactive mode for portal (show selection dialog)"

# --no-spotlight
complete -c coomer -l no-spotlight \
    -d "Disable spotlight mode"

# --debug
complete -c coomer -l debug \
    -d "Enable debug logging"

# --help, -h
complete -c coomer -l help -s h \
    -d "Show help message"
