#!/bin/sh
# Once per account: an AccentColor left in kdeglobals by an older Raye is
# turned into the whole colour scheme it stands for (Raye Night or Day
# for the default violet), so the selection and links get their tuned
# colours back. A picked accent written with older greys is written again.
exec /usr/libexec/raye-accent repair
