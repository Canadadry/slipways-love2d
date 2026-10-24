#!/bin/sh
# usage: love_install.sh

set -eu

packages="love xvfb xauth libgl1-mesa-dri luajit libluajit-5.1-dev pkg-config zip unzip"

. /etc/os-release
if [ "${ID:-}" != ubuntu ] || [ "${VERSION_ID:-}" != 24.04 ]; then
    echo "love_install: supports Ubuntu 24.04 only, found ${PRETTY_NAME:-unknown}" >&2
    exit 1
fi

installed_version() {
    dpkg-query -W -f '${db:Status-Status} ${Version}' "$1" 2>/dev/null | sed -n 's/^installed //p'
}

missing=
for package in $packages; do
    [ -n "$(installed_version "$package")" ] || missing="$missing $package"
done

if [ -n "$missing" ]; then
    sudo=
    [ "$(id -u)" -eq 0 ] || sudo=sudo
    echo "love_install: installing$missing"
    # Minimized images exclude /usr/share/man, and love's postinst fails without its man page.
    echo 'path-include=/usr/share/man/man6/love*' | $sudo tee /etc/dpkg/dpkg.cfg.d/love-man > /dev/null
    $sudo mkdir -p /usr/share/man/man6
    $sudo env DEBIAN_FRONTEND=noninteractive apt-get update -q
    $sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends $packages
else
    echo "love_install: every package installed, skipping apt"
fi

case $(installed_version love) in
    11.5*) ;;
    *)
        echo "love_install: love $(installed_version love) installed, 11.5 required" >&2
        exit 1
        ;;
esac

for package in $packages; do
    echo "$package $(installed_version "$package")"
done
