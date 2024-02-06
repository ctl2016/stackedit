echo "run boot script ......"

setenv loadaddr 0x61000400
setenv get_kernel 'if ab_select slot_name mmc 0#misc; then kernel_name=boot_${slot_name}; echo "boot from kernel: $kernel_name"; else echo "get kernel name failed !"; fi;'
setenv load_kernel_param 'part start mmc 0 $kernel_name kernel_start && part size mmc 0 $kernel_name kernel_size;'
setenv load_kernel 'if run load_kernel_param; then mmc read $loadaddr $kernel_start $kernel_size; else echo "read kernel (name:$kernel_name) failed !"; fi;'

setenv boot_mmc 'if run load_kernel; then bootm $loadaddr -; else echo "load kernel failed !"; fi;'

if run get_kernel; then 
	part number mmc 0 system_${slot_name} part_number
	part size mmc 0 system_${slot_name} part_size
	setexpr part_number fmt %d $part_number
	setexpr part_size fmt %d $part_size
	dm="dm-mod.create=\"system,,,ro, 0 $part_size linear /dev/mmcblk0p${part_number} 0\""
	setenv bootargs "$dm raid=noautodetect boot_slot=$slot_name rootwait dm=system console=ttyAMA0,115200 rootfstype=ext4 root=/dev/dm-0 init=/sbin/init";
	run boot_mmc;
fi;

printenv
