#!/bin/bash

echo 21 > /sys/class/gpio/export
echo in > /sys/class/gpio/gpio21/direction

count=1;

while true
do 
	r=$(cat /sys/class/gpio/gpio21/value)
	if [ "$r" == 1 ]
	then
		(( count = count + 1 ))
	else
		count=0
	fi
	sleep 0.1
	if [ "$count" == 10 ]
	then
		echo "reset button is pushed"
		echo "#!/bin/bash" > ./ConfigureNetwork.sh 
		echo "ifconfig eth0:0 192.168.2.21" >> ./ConfigureNetwork.sh 
					sed -e "s|{IP}|192.168.1.25|g" \
				-e "s|{Gateway}|192.168.1.1|g" \
				-e "s|{DNS}|192.168.1.1|g" \
				dhcpcd.conf.template >"/etc/dhcpcd.conf"
		break
	fi
done;

sleep 2
reboot -f