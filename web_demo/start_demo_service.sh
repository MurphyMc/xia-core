#!/bin/bash

# local script 

killall -9 stock_service.py
killall -9 vs
killall -9 proxy.py 
killall -9 webserver.py
sleep 1

killall -9 stock_service.py
killall -9 vs
killall -9 proxy.py 
killall -9 webserver.py

sleep 1
./stock_service.py &
cd ../XIASocket/sample/
./vs video.ogv &
sleep 1  # need this otherwise it doesn't work

cd ../../proxies
./webserver.py $@ &
sleep 1
./proxy.py 8080 127.0.0.1 &
