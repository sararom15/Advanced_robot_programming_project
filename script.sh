#!/bin/sh
# This is main script
# Source the configuration file and get it 
if [ -r /home/sara/Scrivania/ARP/ConfigurationFile.cfg ]
then
. /home/sara/Scrivania/ARP/ConfigurationFile.cfg
else
echo "Problems with the configuration File"
exit 99
fi

gcc G.c -o G
gcc main.c -o main
./main "$pathlog" "$IP_ADDRESS_PREV" "$IP_ADDRESS_MY" "$IP_ADDRESS_NEXT" "$RF" "$process_G_path" "$portno" "$DEFAULT_TOKEN" "$P_PID_PATH" "$G_PID_PATH" "$waiting_time"
