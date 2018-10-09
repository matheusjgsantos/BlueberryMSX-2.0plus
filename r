#!/bin/bash
FILE1="./msx"
FILE2="./menu"
ITEM=`cat ./item`
MSXBUS1='/romtype1 msxbus'
#amixer cset numid=1 100 > /dev/null
num=1
echo "#!/bin/bash"> msx
echo -n "whiptail --title \"RPMC - Raspberry Pi MSX Clone\" --menu \"Choose a Machine\" 25 78 16 " >> msx
echo -n "" > menu
while machine='' read -r line || [[ -n "$line" ]]; do
	if [[ $line =~ .*$1.* ]]; then
		echo $line >> menu
		echo -n "\"$num\" \"$line\" " >> msx
		num=$((num + 1))
	fi
done < msxmachines

echo -n "--default-item \"\$1\" " >> msx
echo "3>&2 2>&1 1>&3" >> msx 
if [[ $@ =~ .*/rom1.* ]]; then
	MSXBUS1=""
fi
while true;
do
ITEM=`cat ./item`
choice=$(./msx $ITEM)
if [ -z $choice ]; then 
	tput cvvis
	./r0
	exit
fi
a=`sed -n ${choice}p < menu`
echo $choice > ./item
if [[ -n $2 ]]; then 
	echo ./bluemsx-pi /machine \"${a}\" /romtype2 msxbus $2 $3 $4 $5 $6 $7 > xx
	sudo ./xx > /dev/null 2>&1
	#sudo ./bluemsx-pi /machine "$a" /romtype2 msxbus $2 $3 $4 $5 $6 $7
else
	echo ./bluemsx-pi /machine \"${a}\" /romtype1 msxbus /romtype2 msxbus $2 $3 $4 $5 $6 $7 > xx
	sudo ./xx > /dev/null 2>&1
	#sudo ./bluemsx-pi /machine "$a" /romtype1 msxbus /romtype2 msxbus $2 $3
fi
done
