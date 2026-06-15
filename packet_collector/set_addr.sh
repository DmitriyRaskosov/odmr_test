#!/bin/bash
    ip link set dev enp6s0 down
    ip link set dev enp6s0 address 00:55:FF:FF:FF:FF
    ip link set dev enp6s0 up
