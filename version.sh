#!/bin/bash
# Usage: ./version.sh <version>
# Example: ./version.sh 0.5
# Tags the git commit, saves the ROM, and prunes old versions (keeps last 2)

set -e
cd "$(dirname "$0")"

VERSION="$1"
if [ -z "$VERSION" ]; then
    echo "Usage: ./version.sh <version>"
    echo "Example: ./version.sh 0.5"
    exit 1
fi

mkdir -p versions

# Build fresh
mkdir -p build
make clean
mkdir -p build
make

# Copy ROM
cp isogame.gba "versions/isogame-v${VERSION}.gba"
echo "Saved versions/isogame-v${VERSION}.gba"

# Git tag
git add -A
git commit -m "v${VERSION}" --allow-empty
git tag -a "v${VERSION}" -m "Version ${VERSION}"
git push && git push --tags

# Prune: keep only the last 2 ROM files
cd versions
ls -1t isogame-v*.gba 2>/dev/null | tail -n +3 | xargs -r rm -v
cd ..

echo ""
echo "✅ Version ${VERSION} saved. Current versions:"
ls -1t versions/isogame-v*.gba
