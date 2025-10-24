#!/bin/bash

# Script to compare the execution time and output of dcat and cat.

# Usage: ./compare.sh <file> [options...]

if [ "$#" -lt 1 ]; then
    echo "Usage: ./compare.sh <file> [options...]"
    exit 1
fi

FILE=$1
shift
OPTIONS="$@"

DCAT_OUT=$(mktemp)
CAT_OUT=$(mktemp)

# Run dcat
DCAT_TIME=$( ( /usr/bin/time -p ./src/dcat $OPTIONS "$FILE" > "$DCAT_OUT" ) 2>&1 | grep real | awk '{print $2}' )

# Run cat
CAT_TIME=$( ( /usr/bin/time -p cat $OPTIONS "$FILE" > "$CAT_OUT" ) 2>&1 | grep real | awk '{print $2}' )

# Compare outputs
if diff -q "$DCAT_OUT" "$CAT_OUT" >/dev/null; then
    OUTPUT_SAME="yes"
else
    OUTPUT_SAME="no"
fi

# Print results
echo "--- Comparison Summary ---"
echo "File: $FILE"
echo "Options: $OPTIONS"
echo ""
echo "Execution Time:"
echo "  dcat: ${DCAT_TIME}s"
echo "  cat:  ${CAT_TIME}s"
echo ""
echo "Outputs are the same: $OUTPUT_SAME"
echo "-------------------------"

# Clean up
rm "$DCAT_OUT" "$CAT_OUT"
