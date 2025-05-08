#!/bin/bash
# Script to update the version in version.h

if [ $# -ne 1 ]; then
    echo "Usage: $0 <new_version>"
    exit 1
fi

NEW_VERSION=$1
echo "Updating version to $NEW_VERSION"

# Split version into components
IFS='.' read -r MAJOR MINOR PATCH <<< "$NEW_VERSION"

# Update version.h
VERSION_FILE="include/version.h"

# Check if file exists
if [ ! -f "$VERSION_FILE" ]; then
    echo "Error: $VERSION_FILE not found"
    exit 1
fi

# Update the version strings and numbers
sed -i.bak "s/constexpr const char\* VERSION = \"[0-9]*\.[0-9]*\.[0-9]*\"/constexpr const char* VERSION = \"$NEW_VERSION\"/g" "$VERSION_FILE"
sed -i.bak "s/constexpr int VERSION_MAJOR = [0-9]*/constexpr int VERSION_MAJOR = $MAJOR/g" "$VERSION_FILE"
sed -i.bak "s/constexpr int VERSION_MINOR = [0-9]*/constexpr int VERSION_MINOR = $MINOR/g" "$VERSION_FILE"
sed -i.bak "s/constexpr int VERSION_PATCH = [0-9]*/constexpr int VERSION_PATCH = $PATCH/g" "$VERSION_FILE"

# Clean up backup files
rm -f "$VERSION_FILE.bak"

echo "Version updated successfully"
exit 0 