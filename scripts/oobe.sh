#!/bin/bash

#fallback C
LANG=C

#run mx-locale if it exists, in language chooser mode
if [ -x "/usr/bin/mx-locale" ]; then
    /usr/bin/mx-locale --only-lang 2>/dev/null
fi

##source locale if it exists, get real LANG
if [ -e "/etc/default/locale" ]; then
	. /etc/default/locale
fi

#now run localized oobe

LC_ALL=$LANG /usr/sbin/minstall --oobe --config 2>/dev/null

exit 0
