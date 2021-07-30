#!/usr/bin/env bash
set -e
sysctl -w net.ipv4.ip_forward=1
iptables -A FORWARD -i zeth -o ${@:1} -j ACCEPT
iptables -A FORWARD -i ${@:1} -o zeth -m state --state ESTABLISHED,RELATED -j ACCEPT
iptables -t nat -A POSTROUTING -o ${@:1} -j MASQUERADE
