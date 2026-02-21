#!/bin/sh
# Build all C programs in this directory.

set -e
cd "$(dirname "$0")"

echo "Building mx-via-resolvlib..."
cc -o mx-via-resolvlib mx-via-resolvlib.c -lresolv

echo "Building sync-resolution..."
cc -o sync-resolution sync-resolution.c -lresolv

echo "Building async-resolution..."
cc -o async-resolution async-resolution.c -lcares

echo "Done."
