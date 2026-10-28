#!/usr/bin/env python3
"""Copy a UF2 image onto the mass storage drive exposed by the TinyUF2 bootloader.

While the Badger sits in its bootloader it shows up as a removable drive
(``badgerboot`` by default) that firmware images can be dropped onto.
"""

import argparse
import os
import subprocess
import sys


def windows_drives(name):
    command = "(Get-Volume -FileSystemLabel '%s').DriveLetter" % name
    try:
        output = subprocess.check_output(["powershell", "-Command", command], text=True)
    except (OSError, subprocess.CalledProcessError):
        return []
    return [letter.strip() + ":\\" for letter in output.splitlines() if letter.strip()]


def mount_roots():
    if sys.platform == "darwin":
        return ["/Volumes"]
    roots = ["/mnt", "/media", "/run/media"]
    for variable in ("USER", "SUDO_USER"):
        user = os.environ.get(variable)
        if user:
            roots += ["/media/" + user, "/run/media/" + user]
    return roots


def find_drives(name):
    """Return every mounted drive whose name matches, ignoring case."""
    if sys.platform == "win32":
        return windows_drives(name)

    drives = []
    for root in mount_roots():
        if not os.path.isdir(root):
            continue
        for entry in os.listdir(root):
            path = os.path.join(root, entry)
            if entry.lower() == name.lower() and os.path.isdir(path) and path not in drives:
                drives.append(path)
    return drives


def upload(image, drive):
    with open(image, "rb") as source:
        payload = source.read()

    destination = os.path.join(drive, os.path.basename(image))
    try:
        with open(destination, "wb") as target:
            target.write(payload)
            target.flush()
            os.fsync(target.fileno())
    except OSError as error:
        # The board reboots into the new firmware as soon as it has the whole
        # image, which unmounts the drive and can trip up the closing write.
        print("Drive disconnected while finishing the write (%s)." % error)
        print("This normally means the board accepted the image and rebooted.")
        return

    print("Wrote %d bytes to %s" % (len(payload), destination))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", metavar="UF2", help="UF2 image to upload")
    parser.add_argument("-d", "--drive", default="badgerboot",
                        help="name of the bootloader drive (default: badgerboot)")
    args = parser.parse_args()

    if not os.path.isfile(args.image):
        sys.exit("UF2 image not found: %s" % args.image)

    drives = find_drives(args.drive)
    if not drives:
        sys.exit("No drive named '%s' found. Put the board into its bootloader "
                 "and try again." % args.drive)
    if len(drives) > 1:
        print("Multiple '%s' drives found, using %s" % (args.drive, drives[0]))

    upload(args.image, drives[0])


if __name__ == "__main__":
    main()
