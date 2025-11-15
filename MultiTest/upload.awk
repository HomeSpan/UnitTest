#!/usr/bin/awk -f

BEGIN {
	com="/Users/Gregg/Library/Arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool read-mac | grep Connected"
	com | getline
	gsub(":","",$5)
	gsub("-","",$3)
	port=$5
	board=tolower($3)
	if(board=="esp32")
		board="featheresp32"
	com="arduino-cli upload -b esp32:esp32:hs_" board " -p " port " -i build/esp32.esp32.hs_" board "/MultiTest.ino.bin"
	system(com)
}
		
