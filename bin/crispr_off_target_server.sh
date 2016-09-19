#!/bin/bash

echo "Starting Crispr Off-Target server"

DATE=`date +%d-%m-%Y`
cd /nfs/team87/CRISPR-Analyser/
./bin/ots_server -c conf/indexes_nfs.conf >& /var/tmp/ots_logs/ots_$DATE.log &

echo "Crispr off-target server started with process ID: $!"
echo "Logs stored in /var/tmp/ots_logs/ots_$DATE.log"
