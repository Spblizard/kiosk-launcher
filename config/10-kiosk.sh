if [ "$(tty)" = "/dev/tty1" ] && [ -x /usr/bin/kiosk-session ]; then
    /usr/bin/kiosk-session
fi
