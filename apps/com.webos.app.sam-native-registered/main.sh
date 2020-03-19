#!/bin/sh
echo ''
echo '== Step 1 : Export Grobal variables =='
export
sleep 2

echo ''
echo '== Step 2 : Fork Child Process =='
. ./child.sh &
. ./invalid.sh &
sleep 2

echo ''
echo '== Step 3 : Check Resister Native App =='
luna-send -a com.webos.app.sam-native-registered -f -n 2 luna://com.webos.service.applicationmanager/registerApp '{}'
sleep 100
