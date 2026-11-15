#!/bin/sh
# Win+R for the Run box. When raye-shell 0.1 is upgraded inside a running
# session, the shortcut daemon sees the Run box before Spectacle lets go
# of Meta+R and saves an empty shortcut for it in the user's file. That
# empty entry would hide the default (Meta+R) for good: remove it, once.
# A shortcut the user chose is left alone.
value=$(kreadconfig6 --file kglobalshortcutsrc --group services --group org.raye.run.desktop \
    --key _launch --default unset)
if [ -z "$value" ]; then
    kwriteconfig6 --file kglobalshortcutsrc --group services --group org.raye.run.desktop \
        --key _launch --delete
fi
