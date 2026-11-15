# coomer fish completions

# All values are options or output names, never filenames.
complete -c coomer -f

function __coomer_backend_from_cmdline
    set -l words (commandline -opc)
    set -l backend
    for i in (seq 1 (count $words))
        if test "$words[$i]" = --backend
            set -l j (math $i + 1)
            if test $j -le (count $words)
                set backend "$words[$j]"
            end
        else if string match -q -- '--backend=*' "$words[$i]"
            set backend (string replace -- '--backend=' '' "$words[$i]")
        end
    end
    if test -n "$backend"
        echo "$backend"
    end
end

function __coomer_list_monitors
    set -l backend (__coomer_backend_from_cmdline)
    if test "$backend" = portal
        return
    end
    echo all
    set -l args --list-monitors
    if test -n "$backend"
        set -a args --backend "$backend"
    end
    command coomer $args 2>/dev/null \
        | string replace -r -f '^\[[0-9]+\]\s+(\S+).*' '$1'
end

# --backend <mode>: auto|x11|wlr|portal
complete -c coomer -l backend -r -f \
    -a "auto x11 wlr portal" \
    -d "Capture backend (default: auto)"

# --monitor <name>
complete -c coomer -l monitor -r -f \
    -a "(__coomer_list_monitors)" \
    -d "Select monitor/output by name (x11/wlr only, use all for all monitors)"

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

# --version
complete -c coomer -l version \
    -d "Show version"

# --help, -h
complete -c coomer -l help -s h \
    -d "Show help message"
