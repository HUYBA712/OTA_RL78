#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_ADD_INCLUDEDIRS := . 	HW_Interface/BLE \
								HW_Interface/GPIO \
								HW_Interface/Wifi \
								HW_Interface/ExternalWatchDog \
								HW_Interface/Flash_Driver \
								IRO_Handle/ \
								IRO_Handle/IRO_MCU \
								SW_Interface/DateTime \
								SW_Interface/Mqtt \
								SW_Interface/OTA \
								SW_Interface/WifiConfig \
								Utility

COMPONENT_SRCDIRS := . 			HW_Interface/BLE \
								HW_Interface/GPIO \
								HW_Interface/Wifi \
								HW_Interface/ExternalWatchDog \
								HW_Interface/Flash_Driver \
								IRO_Handle/ \
								IRO_Handle/IRO_MCU \
								SW_Interface/DateTime \
								SW_Interface/Mqtt \
								SW_Interface/OTA \
								SW_Interface/WifiConfig \
								Utility