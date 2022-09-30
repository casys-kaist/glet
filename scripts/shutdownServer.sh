#!/bin/bash

backend_urls=('10.0.0.12' '10.0.0.20')

frontend_urls=('10.0.0.12')

# first kill frontends for avoiding TIME_WAIT problem on ports
# which cause 'socket address already in use'
for url in ${frontend_urls[@]}
do
	echo "shutdown frontend in " $url
	ssh $url "pkill frontend"
done 
sleep 2

for url in ${backend_urls[@]}
do
	echo "shutdown backend in " $url
	ssh $url "pkill proxy"
	ssh $url "pkill backend"

done 
sleep 2

