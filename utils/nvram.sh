#!/bin/bash

set -e

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

if [ $# -ne 5 ]; then
    echo "Usage: $0 <input_fd> <output_fd> <vendor_uuid> <var_name> <var_data>"
    echo "Example: $0 ovmf-x86_64-vars.fd modified.fd gBootArgsGuid boot-args 'hello\\0'"
    exit 1
fi

INPUT_FD="$1"
OUTPUT_FD="$2"
VENDOR_UUID="$3"
VAR_NAME="$4"
VAR_DATA="$5"

if [[ ! "$INPUT_FD" = /* ]]; then
    INPUT_FD="$SCRIPT_DIR/$INPUT_FD"
fi
if [[ ! "$OUTPUT_FD" = /* ]]; then
    OUTPUT_FD="$SCRIPT_DIR/$OUTPUT_FD"
fi

if [ ! -f "$INPUT_FD" ]; then
    echo "Error: Input file '$INPUT_FD' does not exist."
    exit 1
fi

if [ ! -x "$SCRIPT_DIR/ovmf.py" ]; then
    echo "Error: ovmf.py is not executable in '$SCRIPT_DIR'. Run 'chmod +x $SCRIPT_DIR/ovmf.py'."
    exit 1
fi

if ! command -v yq &> /dev/null; then
    echo "Error: 'yq' is required but not installed."
    exit 1
fi

TEMP_YAML="$SCRIPT_DIR/temp_$(date +%s).yaml"

echo "Dumping '$INPUT_FD' to YAML..."
"$SCRIPT_DIR/ovmf.py" export "$INPUT_FD" --output-file "$TEMP_YAML" --force
if [ $? -ne 0 ]; then
    echo "Error: Failed to dump '$INPUT_FD' to YAML."
    rm -f "$TEMP_YAML"
    exit 1
fi

echo "Adding variable '$VAR_NAME' with data '$VAR_DATA' to vendor '$VENDOR_UUID'..."
BINARY_DATA=$(echo -n "$VAR_DATA" | sed 's/\\0/\x00/' | base64)
yq -y ".Variables.\"$VENDOR_UUID\".\"$VAR_NAME\" = {\"Data\": \"$BINARY_DATA\", \"Boot Access\": true, \"Runtime Access\": true}" "$TEMP_YAML" > "$TEMP_YAML.tmp" && mv "$TEMP_YAML.tmp" "$TEMP_YAML"
if [ $? -ne 0 ]; then
    echo "Error: Failed to modify YAML file."
    rm -f "$TEMP_YAML" "$TEMP_YAML.tmp"
    exit 1
fi

echo "Compiling modified YAML to '$OUTPUT_FD'..."
"$SCRIPT_DIR/ovmf.py" compile "$TEMP_YAML" "$OUTPUT_FD" --force
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile YAML to '$OUTPUT_FD'."
    rm -f "$TEMP_YAML"
    exit 1
fi

rm -f "$TEMP_YAML"
echo "Successfully created '$OUTPUT_FD' with new variable '$VAR_NAME'."

exit 0