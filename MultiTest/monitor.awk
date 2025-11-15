#!/usr/bin/awk -f

BEGIN {
	com="/Users/Gregg/Library/Arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool read-mac | grep Connected"
	com | getline
	gsub(":","",$5)
	port=$5
	com="arduino-cli monitor -p " port "  -c 115200"
	system(com)
}
		
