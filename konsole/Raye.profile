[General]
Name=Raye
Parent=FALLBACK/
# The shell starts in Raye Night or Raye Day, whichever the desktop uses
# when the tab opens: when the window colour of the desktop is light, the
# start command asks Konsole for RayeDay (escape 50, as konsoleprofile
# does), and for RayeNight when it is dark. A scheme the user picked
# for this profile in Konsole is left alone. Then the shell runs as usual.
Command=/bin/sh -c '( [ -n "$KONSOLE_DBUS_SESSION" ] || exit; c=$(kreadconfig6 --group Colors:Window --key BackgroundNormal); case $c in *[!0-9,]*|"") exit;; esac; IFS=,; set -- $c; [ $# -eq 3 ] || exit; if [ $(( (2126*$1+7152*$2+722*$3)/10000 )) -gt 127 ]; then t=RayeDay w="0;15"; else t=RayeNight w="15;0"; fi; [ "$COLORFGBG" = "$w" ] && exit; p=$(qdbus6 "$KONSOLE_DBUS_SERVICE" "$KONSOLE_DBUS_SESSION" profile); s=; case $p in */*|"") ;; *) f=${XDG_DATA_HOME:-$HOME/.local/share}/konsole/$p.profile; [ -f "$f" ] && s=$(kreadconfig6 --file "$f" --group Appearance --key ColorScheme);; esac; case $s in ""|RayeNight|RayeDay) printf "\\033]50;ColorScheme=%s\\007" "$t";; esac ) 2>/dev/null; exec "${SHELL:-/bin/bash}"'
TerminalColumns=120
TerminalRows=30
TerminalMargin=8
TerminalCenter=false

[Appearance]
ColorScheme=RayeNight
Font=Cascadia Mono,11,-1,5,400,0,0,0,0,0,0,0,0,0,0,1
LineSpacing=1
BoldIntense=true

[Cursor Options]
# the thin bar of the Windows terminal (0 block, 1 bar, 2 underline)
CursorShape=1

[Scrolling]
HistoryMode=2
ScrollBarPosition=1

[Interaction Options]
AutoCopySelectedText=false
CopyTextAsHTML=false
TrimTrailingSpacesInSelectedText=true
