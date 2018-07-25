# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Systems to build
linux=true
windows=true
osx=true

# Other Basic variables
VERSION=
proc=2
mem=2000
scriptName=$(basename -- "$0")

# Help Message
read -d '' usage <<- EOF
Usage: $scriptName [-b|s|o|h|j|m|] version

Arguments:
version        Version number, commit, or branch to build. If building a commit or branch, the -c option must be specified

Options:
-b|--build    Do a gitian build
-o|--os        Specify which Operating Systems the build is for. Default is lwx. l for linux, w for windows, x for osx
-j        Number of processes to use. Default 2
-m        Memory to allocate in MiB. Default 2000
--setup         Setup the gitian building environment. Only works on Debian-based systems (Ubuntu, Debian)
-h|--help    Print this help message
EOF

# Get options and arguments
while :; do
    case $1 in
    # Build
    -b|--build)
        build=true
        ;;
    # Operating Systems
    -o|--os)
    if [ -n "$2" ]
        then
        linux=false
        windows=false
        osx=false
        if [[ "$2" = *"l"* ]]
        then
            linux=true
        fi
        if [[ "$2" = *"w"* ]]
        then
            windows=true
        fi
        if [[ "$2" = *"x"* ]]
        then
            osx=true
        fi
        shift
    else
        echo 'Error: "--os" requires an argument containing an l (for linux), w (for windows), or x (for Mac OSX)\n'
        exit 1
    fi
    ;;
    # Help message
    -h|--help)
        echo "$usage"
        exit 0
    ;;
    # Number of Processes
    -j)
    if [ -n "$2" ]
    then
        proc=$2
        shift
    else
        echo 'Error: "-j" requires an argument'
        exit 1
    fi
    ;;
    # Memory to allocate
    -m)
    if [ -n "$2" ]
    then
        mem=$2
        shift
    else
        echo 'Error: "-m" requires an argument'
        exit 1
    fi
    ;;
    # Setup
    --setup)
        setup=true
    ;;
    *) # Default case: If no more options then break out of the loop.
        break
    esac
    shift
done

#Detect distribution and platform
if [ -f /etc/os-release ]; then
    # freedesktop.org and systemd
    . /etc/os-release
    HOSTOS=$NAME
    HOSTVER=$VERSION_ID
elif type lsb_release >/dev/null 2>&1; then
    # linuxbase.org
    HOSTOS=$(lsb_release -si)
    HOSTOS=$(lsb_release -sr)
elif [ -f /etc/lsb-release ]; then
    # For some versions of Debian/Ubuntu without lsb_release command
    . /etc/lsb-release
    HOSTOS=$DISTRIB_ID
    HOSTOS=$DISTRIB_RELEASE
elif [ -f /etc/debian_version ]; then
    # Older Debian/Ubuntu/etc.
    HOSTOS=Debian
    HOSTOS=$(cat /etc/debian_version)
elif [ -f /etc/SuSe-release ]; then
    # Older SuSE/etc.
    HOSTOS=SuSe
    HOSTVER=unknown
elif [ -f /etc/redhat-release ]; then
    # Older Red Hat, CentOS, etc.
    HOSTOS=redhat
    HOSTVER=unknown
else
    # Fall back to uname, e.g. "Linux <version>", also works for BSD, etc.
    HOSTOS=$(uname -s)
    HOSTVER=$(uname -r)
fi

# Set up LXC
export USE_LXC=1
export LXC=1
export LXC_BRIDGE=lxcbr0

# Check for OSX SDK
if [[ ! -e "gitian-builder/inputs/MacOSX10.11.sdk.tar.gz" && $osx == true ]]
then
    echo "Cannot build for OSX, SDK does not exist. Will build for other OSes"
    osx=false
fi

# Get version
if [[ -n "$1" ]]
then
    VERSION=$1
    COMMIT=$VERSION
    shift
fi

# Check that a version is specified
if [[ $VERSION == "" ]]
then
    echo "$scriptName: Missing version."
    echo "Try $scriptName --help for more information"
    exit 1
fi

COMMIT="v${VERSION}"
echo ${COMMIT}

# Setup build environment
if [[ $setup = true ]]
then
    if [[ HOSTOS = "Ubuntu" ]]
    then
        sudo apt-get install ruby apache2 git apt-cacher-ng python-vm-builder qemu-kvm qemu-utils curl

        if [[ HOSTVER = *"18"* ]]
        then
            sudo apt-get install lxc
        else
            if [[ HOSTVER = *"16"* ]]
            then
                sudo apt-get install -t xenial-backports lxc
            else
                if [[ HOSTVER = *"14"* ]]
                then
                    sudo apt-get install -t trusty-backports lxc
                else
                    echo "Automated setup on ${HOSTOS} ${HOSTVER} not possible."
                fi
            fi
        fi

        git clone https://github.com/gulden/gitian-builder.git
        pushd ./gitian-builder
        bin/make-base-vm --suite bionic --arch amd64 --lxc
        popd
        sudo ifconfig lxcbr0 up 10.0.2.2
    else
        echo "Automated setup on ${HOSTOS} ${HOSTVER} not possible, please setup manually for building, steps are as follows:"
        echo "install ruby apache2 git apt-cacher-ng python-vm-builder qemu-kvm qemu-utils curl lxc"
        echo "git clone https://github.com/gulden/gitian-builder.git"
        echo "pushd ./gitian-builder"
        echo "bin/make-base-vm --suite bionic --arch amd64 --lxc"
        echo "popd"
        echo "sudo ifconfig lxcbr0 up 10.0.2.2"
    fi
fi

# Build
if [[ $build = true ]]
then
    # Make output folder
    mkdir -p ./gulden-binaries/${VERSION}

    # Download Dependencies
    echo ""
    echo "Downloading Dependencies"
    echo ""
    pushd ./gitian-builder
    mkdir -p inputs
    make -C ../depends download SOURCES_PATH=`pwd`/cache/common

    # Linux
    if [[ $linux = true ]]
    then
            echo ""
        echo "Compiling ${VERSION} Linux"
        echo ""
        ./bin/gbuild -j ${proc} -m ${mem} --commit Gulden=${COMMIT} --url Gulden=`pwd`/../ ../contrib/gitian-descriptors/gitian-linux.yml
        mv build/out/gulden-*.tar.gz build/out/src/gulden-*.tar.gz ../gulden-binaries/${VERSION}
    fi
    # Windows
    if [[ $windows = true ]]
    then
        echo ""
        echo "Compiling ${VERSION} Windows"
        echo ""
        ./bin/gbuild -j ${proc} -m ${mem} --commit Gulden=${COMMIT} --url Gulden=.. ../contrib/gitian-descriptors/gitian-win.yml
        mv build/out/gulden-*.zip build/out/gulden-*.exe ../bitcoin-binaries/${VERSION}
    fi
    # Mac OSX
    if [[ $osx = true ]]
    then
        echo ""
        echo "Compiling ${VERSION} Mac OSX"
        echo ""
        ./bin/gbuild -j ${proc} -m ${mem} --commit GUlden=${COMMIT} --url Gulden=.. ../contrib/gitian-descriptors/gitian-osx.yml
        mv build/out/gulden-*.tar.gz build/out/gulden-*.dmg ../gulden-binaries/${VERSION}
    fi
    popd

fi

