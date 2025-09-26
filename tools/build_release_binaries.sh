#!/usr/bin/env bash

set -e

SCRIPT_DIR="$(dirname "$(readlink -f "$BASH_SOURCE")")"
REPO_ROOT="$(dirname $SCRIPT_DIR)"
DEMO_DIRECTORY="$REPO_ROOT/demo"
BINARIES_PATH="$REPO_ROOT/release-binaries"
PLATFORMS="tnrfl"

print_help() {
    cat <<EOF >&2
NAME
    $0 - Prepare T-Mobile binaries for release.

SYNOPSIS
    $0 [ OPTIONS... ]

OPTIONS
    -v, --client-version CLIENT_VERSION
            -  anjay-zephyr-client version.
    -a, --anjay-version ANJAY_VERSION
            - Anjay version.
    -c, --clean
            - clean the whole repository before building binaries.
    -z, --zephyrproject ZEPHYRPROJECT_PATH
            - absolute path to the zephyrproject/ directory.
    -p  --platforms
            - platforms to build: (t)hingy91, (n)rf9160dk, n(r)f7002dk, nr(f)9151dk,
              (l)475.
              default: all
    -h, --help
            - print this message and exit.
EOF
}

while [[ $# > 0 ]]; do
    case "$1" in
        -v|--version)               CLIENT_VERSION="$2"; shift ;;
        -a|--anjay-version)         ANJAY_VERSION="$2"; shift ;;
        -z|--zephyrproject)         ZEPHYRPROJECT_PATH="$2"; shift ;;
        -p|--platforms)             PLATFORMS="$2"; shift ;;
        -c|--clean)                 CLEAN_REPO=1 ;;
        -h|--help)
            print_help
            exit 0
            ;;
        *)
            echo "unrecognized option: $1; use -h or --help for help"
            exit 1
            ;;
    esac

    shift
done

set_west_manifest_path() {
    local WEST_PATH="$1"
    west config manifest.path $WEST_PATH
}

set_west_manifest_file() {
    local WEST_FILE="$1"
    west config manifest.file $WEST_FILE
}

update_west() {
    west update -n -o=--depth=1 || west update -n -o=--depth=1
}

set_anjay_version() {
    sed -i -e "s/26.09/$ANJAY_VERSION/g" $ANJAY_ZEPHYR_ROOT/deps/anjay/src/core/anjay_core.c
}

if [[ -z "$CLIENT_VERSION" || -z "$ANJAY_VERSION" || -z "$ZEPHYRPROJECT_PATH" ]]; then
    echo -e "\e[91mAnjay version, client version or zephyrproject/ path not specified.\e[0m"
    echo "Use $0 --help for more info."
    exit 1
fi

ANJAY_ZEPHYR_ROOT="$ZEPHYRPROJECT_PATH/modules/lib/anjay-internal"

if [ -n "$CLEAN_REPO" ]; then
    git clean -ffdx && \
            git submodule foreach --recursive git clean -ffdx && \
            git submodule update --init --recursive && \
            git submodule foreach --recursive git reset --hard && \
            git checkout -- .
fi

MCUBOOT_VERSION="$("$SCRIPT_DIR/generate_mcuboot_version.py" "$CLIENT_VERSION")"
sed -i -e "s/^# DEPLOY: CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION\$/CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"$MCUBOOT_VERSION\"/" */boards/*.conf
sed -i -e "s/^# DEPLOY: CONFIG_MCUBOOT_EXTRA_IMGTOOL_ARGS\$/CONFIG_MCUBOOT_EXTRA_IMGTOOL_ARGS=\"--version $MCUBOOT_VERSION\"/" */boards/*.conf

mkdir -p "$BINARIES_PATH"

# nRF9160DK ####################################################################
if [[ "$PLATFORMS" == *n* ]]; then
    mkdir -p "$BINARIES_PATH/nRF9160DK"
    set_west_manifest_path $DEMO_DIRECTORY
    set_west_manifest_file "west-nrf.yml"
    update_west
    set_anjay_version

    pushd $DEMO_DIRECTORY
        west build -b nrf9160dk/nrf9160/ns -p -- \
                -DCONFIG_ANJAY_ZEPHYR_VERSION="\"$CLIENT_VERSION.0\""
        cp build/zephyr/merged.hex "$BINARIES_PATH/nRF9160DK/demo_nrf9160dk_merged.hex"
    popd
fi

# nRF7002DK ####################################################################
if [[ "$PLATFORMS" == *r* ]]; then
    mkdir -p "$BINARIES_PATH/nRF7002DK"
    set_west_manifest_path $DEMO_DIRECTORY
    set_west_manifest_file "west-nrf.yml"
    update_west
    set_anjay_version

    pushd $DEMO_DIRECTORY
        west build -b nrf7002dk/nrf5340/cpuapp/ns -p -- \
                -DCONFIG_ANJAY_ZEPHYR_VERSION="\"$CLIENT_VERSION.0\""
        cp build/zephyr/merged.hex "$BINARIES_PATH/nRF7002DK/demo_nrf7002dk.hex"
    popd
fi


# Thingy:91 ####################################################################
if [[ "$PLATFORMS" == *t* ]]; then
    mkdir -p "$BINARIES_PATH/Thingy91"
    set_west_manifest_path $DEMO_DIRECTORY
    set_west_manifest_file "west-nrf.yml"
    update_west
    set_anjay_version

    pushd $DEMO_DIRECTORY
        west build -b thingy91/nrf9160/ns -p -- \
                -DCONFIG_ANJAY_ZEPHYR_VERSION="\"$CLIENT_VERSION.0\""
        cp build/zephyr/merged.hex "$BINARIES_PATH/Thingy91/demo_thingy91_app_signed.hex"
    popd
fi

# nRF9151DK ####################################################################
if [[ "$PLATFORMS" == *n* ]]; then
    mkdir -p "$BINARIES_PATH/nRF9151DK"
    set_west_manifest_path $DEMO_DIRECTORY
    set_west_manifest_file "west-nrf.yml"
    update_west
    set_anjay_version

    pushd $DEMO_DIRECTORY
        west build -b nrf9151dk/nrf9151/ns -p -- \
                -DCONFIG_ANJAY_ZEPHYR_VERSION="\"$CLIENT_VERSION.0\""
        cp build/zephyr/merged.hex "$BINARIES_PATH/nRF9151DK/demo_nrf9151dk_merged.hex"
    popd
fi

# L475 #########################################################################
if [[ "$PLATFORMS" == *l* ]]; then
    mkdir -p "$BINARIES_PATH/B-L475-IOT01A"
    set_west_manifest_path $DEMO_DIRECTORY
    set_west_manifest_file "west.yml"
    update_west
    set_anjay_version

    pushd $DEMO_DIRECTORY
        west build -b disco_l475_iot1 --sysbuild -p -- \
                -DCONFIG_ANJAY_ZEPHYR_VERSION="\"$CLIENT_VERSION.0\""
        cp build/demo/zephyr/zephyr.signed.bin "$BINARIES_PATH/B-L475-IOT01A/demo_B-L475E-IOT01A1_merged.bin"
        cp build/zephyr/merged.hex "$BINARIES_PATH/B-L475-IOT01A/demo_B-L475E-IOT01A1_merged.hex"
    popd
fi
