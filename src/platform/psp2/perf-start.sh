#!/bin/sh
IP=$1
shift

# Kill any previous session
echo | nc $IP 7215 > /dev/null
echo | nc $IP 7216 > /dev/null

curl -sT perf.bin ftp://$IP:1337/ux0:app/MGBA00002/
echo launch MGBA00002 | nc $IP 1338 > /dev/null

sleep 2

(for ARG in $@; do
	echo $ARG
done; echo) | nc $IP 7215
exit 0
