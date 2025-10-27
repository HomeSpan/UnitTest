
boards=(
	hs_esp32s2
	hs_featheresp32
	hs_esp32s3
	hs_esp32c3
	hs_esp32c5
	hs_esp32c6
	)

core=$(arduino-cli core list | grep esp32)

for board in ${boards[@]}; do

	echo Compiling for $board under ${core}
	arduino-cli compile -b esp32:esp32:$board --build-property build.partitions=min_spiffs --build-property upload.maximum_size=1966080 --warnings all --clean &> $board.out

done


