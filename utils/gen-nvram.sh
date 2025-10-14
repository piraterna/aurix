#!/bin/bash

if [[ -z $1 ]]; then
    printf "Please don't invoke this script manually. Run \`make nvram\` instead.\n"
    exit 1
fi

output_file=""
variables=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -o)
            output_file="$2"
            shift 2
            ;;
        -var,*)
            var_arg="${1#-var,}"
            IFS=',' read -r -a var_args <<< "$var_arg"
            guid=""
            name=""
            data=""
            attr=""
            for arg in "${var_args[@]}"; do
                IFS='=' read -r key value <<< "$arg"
                if [[ "$key" == "data" && "$value" =~ ^\"(.*)\"$ ]]; then
                    value="${BASH_REMATCH[1]}"
                fi
                case "$key" in
                    guid)
                        guid="$value"
                        ;;
                    name)
                        name="$value"
                        ;;
                    data)
                        data="$value"
                        ;;
                    attr)
                        attr="$value"
                        ;;
                    *)
                        printf "Unknown variable key: %s\n" "$key"
                        exit 1
                        ;;
                esac
            done
            if [[ -z "$guid" || -z "$name" || -z "$data" || -z "$attr" ]]; then
                printf "Error: Missing required fields in -var argument: %s\n" "$var_arg"
                exit 1
            fi
            hex_data=$(echo -ne "$data" | xxd -p -c 256 | tr -d '\n')
            variables+=("{\"name\":\"$name\",\"guid\":\"$guid\",\"attr\":$attr,\"data\":\"$hex_data\"}")
            shift 1
            ;;
        -h|--help)
            printf "Usage: %s -o <output_file> [-var,<key>=<value>,...]\n" "$0"
            printf "Options:\n"
            printf "  -o <file>               Specify output JSON file (QEMU uefi-vars style)\n"
            printf "  -var,<key>=<value>,...  Specify NVRAM variable (guid, name, data, attr)\n"
            printf "  -h, --help              Display this help message\n"
            printf "Example:\n"
            printf "  %s -o uefi_nvram.json -var,guid=...,name=boot-args,data=\"test\\0\",attr=7\n" "$0"
            exit 0
            ;;
        *)
            printf "Unknown option: %s\n" "$1"
            exit 1
            ;;
    esac
done

if [[ -z $output_file ]]; then
    printf "Error: Output file not specified with -o\n"
    exit 1
fi

json_output="{\"version\":2,\"variables\":[$(IFS=','; echo "${variables[*]}")]}"

echo "$json_output" > "$output_file"

if command -v jq >/dev/null 2>&1; then
    jq . "$output_file" > tmp.json && mv tmp.json "$output_file"
fi

exit 0