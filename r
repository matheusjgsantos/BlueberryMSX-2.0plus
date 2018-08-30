#!/bin/bash
num=1
FILE1="./msx"
FILE2="./menu"
ITEM=`cat ./item`
echo "#!/bin/bash"> msx
echo -n "whiptail --title \"MSX Machines\" --menu \"Choose a Machine\" 25 78 16 " >> msx
echo -n "" > menu
while machine='' read -r line || [[ -n "$line" ]]; do
	i=0
	mach=$(echo $line | tr "/" "\n")
#	echo $mach
	IFS=$'/'
	for l in $line; do
		i=$((i + 1))
		if [ $i == "2" ]
		then
			echo $l >> menu
#			l=${l/(a)/[a]}
#			l=${l/(b)/[b]}
			echo -n "\"$num\" \"$l\" " >> msx
			num=$((num + 1))
		fi
	done
	
	IFS=''
done < m

echo -n "--default-item \"$ITEM\" " >> msx
echo "3>&2 2>&1 1>&3" >> msx

choice=$(./msx)
a=`sed -n ${choice}p < menu`
echo $choice > ./item
echo ./bluemsx-pi /machine \"${a}\" /romtype1 msxbus /romtype2 msxbus > xx
sudo ./bluemsx-pi /machine "$a" /romtype1 msxbus /romtype2 msxbus $@

