#!/bin/bash

# To avoid sudo password, change the sudo config using 'sudo visudo' and 
#   add this line to the bottom:   "xilinx ALL=(ALL) NOPASSWD: ALL"
sudo chown -R xilinx:xilinx /home/xilinx/jupyter_notebooks/xvc_demo

# This will get the directory of 'this' *.sh script, disregarding the
# current working directory of the calling / parent process...
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
echo -e "Script directory: $SCRIPT_DIR\n"

orig_dir=$(pwd)
cd $SCRIPT_DIR/xvc_server

addr="0x43C00000"   # default XVC base address, when no -a argument given
port="2542"         # default XVC port, when no -p argument given

# Kills the oldest running instance,
if [[ $# == 1 ]] && [[ $1 == 'kill' ]]; then
    ps_aux=$(ps aux | grep -v 'grep' | grep 'xvcServer' -m 1)
    xvc_pid=$(echo $ps_aux | awk -F ' ' '{print $2}')
    xvc_cmd=$(echo $ps_aux | awk -F 'sudo' '{print $2}')
    if [[ $xvc_pid != "" ]]; then
        echo -e "Killing process:  $xvc_cmd  |  [pid: $xvc_pid]"
        sudo kill $xvc_pid
    else
        echo -e "\nNo running instance of xvcServer* found...\n"
    fi
    # User just wanted to kill running program
    exit 0
elif [[ $# == 1 ]]; then
    addr=$1      # user specified base address
elif [[ $# == 2 ]]; then
    addr=$1      # user specified base address
    port=$2      # user specified XVC port
elif [[ $# == 0 ]]; then
    echo -e "\nUsing default values:"
    echo -e "        [debug bridge base address (hex)] : '$addr'"
    echo -e "                               [xvc port] : '$port'"
else
    echo -e "\nUsage:"
    echo -e "    "
    echo -e "    ./start_xvc.sh [debug bridge base address (hex)] [xvc port]"
    echo -e "    "
    echo -e "    The xvcServer program uses /dev/mem for AXI memory mapped IP"
    echo -e "    access. Use extreme caution here..."
    echo -e "    "
    echo -e "    Note: arguments are optional and default to the following:"
    echo -e "        [debug bridge base address (hex)] : '$addr'"
    echo -e "                               [xvc port] : '$port'"
    echo -e "    "
    exit 0
fi

# Ensure $addr can be converted to a valid 64-bit hex value...
(( 64#$addr ))
if [[ $? != '0' ]]; then
    echo -e "Error address could not be parsed as 32-bit hexadecimal string  |  \$addr: '$addr'\n"
    exit 1
fi
# Ensure $port can be converted to a valid 32-bit int value...
(( 32#$port ))
if [ $? != '0' ]
then
    echo "ERROR: [xvc port] parameter must be a valid integer.\n"
    echo $USAGE
    exit 1
fi

# Build the xvcServerPynq user-space application...
make

echo -e "\naddr: $addr\nport: $port\n"

sudo chown -R xilinx:xilinx /home/xilinx/jupyter_notebooks/xvc_demo

killall -q -r sudo xvcServer*

echo -e ""
xvc_app_cmd="sudo ./xvcServer -a $addr -p $port &"
echo $xvc_app_cmd
eval $xvc_app_cmd

echo -e ""

cd $orig_dir
