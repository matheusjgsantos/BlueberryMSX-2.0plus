#!/bin/bash
num=1
FILE1="./msx"
FILE2="./menu"
ITEM=`cat ./item`
MSXBUS1='/romtype1 msxbus'
echo "#!/bin/bash"> msx
echo -n "whiptail --title \"MSX Machines\" --menu \"Choose a Machine\" 25 78 16 " >> msx
echo -n "" > menu
while machine='' read -r line || [[ -n "$line" ]]; do
	i=0
	mach=$(echo $line | tr "/" "\n")
	IFS=$'/'
	for l in $line; do
		i=$((i + 1))
		if [[ $i -eq "2" && $l =~ .*$1.* ]]; then
			echo $l >> menu
			echo -n "\"$num\" \"$l\" " >> msx
			num=$((num + 1))
		fi
	done
	IFS=''
done < m

echo -n "--default-item \"$ITEM\" " >> msx
echo "3>&2 2>&1 1>&3" >> msx
if [[ $@ =~ .*/rom1.* ]]; then
	MSXBUS1=""
fi
choice=$(./msx)
if [ -z $choice ]; then 
	exit
fi
a=`sed -n ${choice}p < menu`
echo $choice > ./item
echo ./bluemsx-pi /machine \"${a}\" /romtype1 msxbus /romtype2 msxbus > xx
if [[ -z $MSXBUS1 ]]; then 
	sudo ./bluemsx-pi /machine "$a" /romtype2 msxbus $2 $3
else
	sudo ./bluemsx-pi /machine "$a" /romtype1 msxbus /romtype2 msxbus $2 $3
fi

