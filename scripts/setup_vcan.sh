#!/bin/bash
# Setup two virtual CAN interfaces with bidirectional gateway
# Messages sent on vcan0 appear on vcan1 and vice versa
# Requires: can-utils (cangw), iproute2
# Usage: ./setup_vcan.sh        (re-runs itself under sudo when not root)

set -e

# modprobe and "ip link add" need root, so rerun the whole script under sudo
# rather than failing partway through and leaving half the setup in place.
if [[ $EUID -ne 0 ]]; then
    if ! command -v sudo > /dev/null 2>&1; then
        echo "Error: root privileges are required and sudo was not found." >&2
        echo "       Run this script as root instead." >&2
        exit 1
    fi
    echo "Root privileges required, re-running under sudo..." >&2
    exec sudo -- "$(readlink -f "$0")" "$@"
fi

modprobe vcan
modprobe can-gw

# Create interfaces
ip link add dev vcan0 type vcan
ip link add dev vcan1 type vcan

ip link set up vcan0
ip link set up vcan1

# Bidirectional gateway: vcan0 <-> vcan1
# A rule without -X forwards Classical CAN only; CAN FD frames need their own
# -X rule or they are silently dropped by the gateway.
cangw -A -s vcan0 -d vcan1 -e
cangw -A -s vcan1 -d vcan0 -e
cangw -A -s vcan0 -d vcan1 -e -X
cangw -A -s vcan1 -d vcan0 -e -X

echo "vcan0 <-> vcan1 gateway active (Classical CAN + CAN FD)"
echo "Test: cansend vcan0 123#DEADBEEF     ->  candump vcan1"
echo "      cansend vcan0 123##1DEADBEEF   ->  candump vcan1   (CAN FD)"
echo
echo "Note: cansend refuses CAN FD unless the interface MTU is exactly 72."
echo "      Recent kernels default vcan to MTU 2060 (CAN XL); if FD sends fail:"
echo "        ip link set vcan0 mtu 72 && ip link set vcan1 mtu 72"
