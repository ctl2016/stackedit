#!/bin/bash
lpdev=""
SECTOR_SIZE=512 # block size 512B
ALIGN_SECT=2048 # align sector count

part_size_bytes() {
	# 1sector: 512B -> 2048sector: 1M
	local MB=$1
	echo $((SECTOR_SIZE * $ALIGN_SECT * $MB))
}

PARTS_INFO=(
	misc:a004:$(part_size_bytes 1):"raw":""                   # 1M
	bootscr:a009:$(part_size_bytes 1):"raw":"boot.scr"        # 1M
	vbmeta_a:a009:$(part_size_bytes 1):"raw":"vbmeta.img"     # 1M
	vbmeta_b:a009:$(part_size_bytes 1):"raw":"vbmeta.img"     # 1M
	boot_a:a000:$(part_size_bytes 13):"raw":"boot.img"        # 13M
	boot_b:a000:$(part_size_bytes 13):"raw":"boot.img"        # 13M
	system_a:a006:$(part_size_bytes 135):"ext4":"rootfs.ext4" # 128M
	system_b:a006:$(part_size_bytes 135):"ext4":"rootfs.ext4" # 128M
	test_a:a009:$(part_size_bytes 1):"ext4":""                # 1M
	test_b:a009:$(part_size_bytes 1):"ext4":""                # 1M
	userdata:a009:0:"ext4":""                                 # other all space
)

function mk_ramdisk() {
	# rootfs
	cd rootfs-ramfs
	rm ../rootfs.cpio.gz
	find ./ | cpio -o --format=newc | gzip -9 > ../rootfs.cpio.gz
	cd ..
}

function create_vbmeta_img() {

    local ret=0

    if [ -s boot.img ];then
        #./avbtool add_hash_footer --partition_name boot --dynamic_partition_size  --image boot.img
        ./avbtool add_hash_footer --partition_name boot --partition_size 13631488 --key private_key.pem --algorithm SHA256_RSA4096 --image boot.img
        ret=$((ret+1))
    fi

    if [ -s rootfs.ext4 ];then
        ./avbtool add_hashtree_footer --partition_name system --do_not_generate_fec --key private_key.pem --algorithm SHA256_RSA4096 --image rootfs.ext4
        ret=$((ret+1))
    fi

    if [ $ret -eq 2 ];then
        ./avbtool make_vbmeta_image --algorithm SHA256_RSA4096 --key private_key.pem --include_descriptors_from_image boot.img --include_descriptors_from_image rootfs.ext4 --output vbmeta.img --padding_size 4096
    fi

    ./avbtool info_image --image vbmeta.img
}

mk_sdcard() {

	local total_size=$((1024 * 1024 * 1024))

	dd if=/dev/zero of=sdcard.img bs=1M count=$(( total_size / (1024*1024) ))
	[[ $? -ne 0 ]] && return $?
	
	local i=0
	local start_sect=$ALIGN_SECT
	local end_sect=$START_SECT
	local bytes=$(($start_sect * $SECTOR_SIZE))

	for i in ${!PARTS_INFO[*]}; do
		local part=${PARTS_INFO[$i]}
		local nm=$(echo "$part" | awk -F":" '{print $1}')
		local ty=$(echo "$part" | awk -F":" '{print $2}')
		local sz=$(echo "$part" | awk -F":" '{print $3}')
		local sz_0=$(($sz+0))
		[[ $sz_0 -eq 0 ]] && sz=$(( $total_size - $bytes ))
		bytes=$(($bytes + $sz))
		[[ $bytes -gt $total_size ]] && echo "error: parts size ($bytes) is larger than total($total_size) size !" && exit

		local number=$(($i+1))
		end_sect=$(($start_sect + $sz/SECTOR_SIZE - 1))
		echo
		echo "<< number: $number, name: $nm, sec: $start_sect - $end_sect, size:$(( $sz/(1024*1024) ))M >>"
		echo

		if [ $sz_0 -eq 0 ];then
			sgdisk -N $number -c $number:$nm -t $number:$ty -v sdcard.img
		else
			sgdisk -n $number:$start_sect:$end_sect -c $number:$nm -t $number:$ty -v sdcard.img
		fi
		[[ $? -ne 0 ]] && exit $?

		start_sect=$(($end_sect + 1))
	done

	return 0
}

err_exit() {
	if [ $1 -ne 0 ];then
		[[ -n $lpdev ]] && losetup -d $lpdev
		exit 1
	fi
}

function build() {

	local zImg="./zImage"
	local dtb="./vexpress-v2p-ca9.dtb"
	local bootscr="./boot.scr"

	cp ../output/images/rootfs.cpio.gz ./
	cp ../output/images/boot.scr ./
	cp ../output/images/rootfs.ext2 ./
	cp ../output/images/rootfs.ext4 ./
	cp ../output/images/u-boot ./
	cp ../output/images/vexpress-v2p-ca9.dtb ./
	cp ../output/images/zImage ./

	mk_ramdisk
	echo 
	echo "file rootfs.cpio.gz"
	echo 
	file rootfs.cpio.gz

	echo 
	echo "make android boot.img"
	echo 
	rm ./boot.img
	./aboot/bin/mkbootimg --kernel $zImg --dt $dtb --ramdisk ./rootfs.cpio.gz --base 0x71000000 -o ./boot.img
	[[ $? -ne 0 ]] && return
	file ./boot.img

    # create vbmeta.img
    create_vbmeta_img

	# make boot.scr

	echo 
	echo "make boot.scr"
	echo 
	if [ -n "$bootscr" ];then
		rm "$bootscr"
		mkimage -C none -A arm -T script -d boot.cmd $bootscr
		[[ $? -ne 0 ]] && return
	fi
	
	file $bootscr

	# make sdcard
	echo 
	echo "make sdcard"
	echo 
	mk_sdcard
	err_exit $?

	# mount sdcard
	echo 
	echo "mount sdcard"
	echo 
	mkdir -p ./mnt
	lpdev=$(losetup -f)
	losetup -P $lpdev ./sdcard.img
	err_exit $?

	for i in ${!PARTS_INFO[*]}; do
		local idx=$(($i+1))
		local part=${PARTS_INFO[$i]}
		local name=$(echo "$part" | awk -F":" '{print $1}')
		local type=$(echo "$part" | awk -F":" '{print $4}')
		local file=$(echo "$part" | awk -F":" '{print $5}')

		[[ ! -b ${lpdev}p${idx} ]] && echo "error: ${lpdev}p${idx} not exist !" &&  err_exit 1

		echo 
		echo "make p${idx}:name($name):(file:$file)"
		echo 

		if [[ "$type" =~ "ext"  ]];then
			mkfs.$type ${lpdev}p${idx}
			err_exit $?
			fsck.$type -y ${lpdev}p${idx}
			err_exit $?
		fi

		if [ -s "$file" ];then
			dd if=$file of=${lpdev}p${idx}
			err_exit $?

			if [[ "$type" =~ "ext"  ]];then
				resize2fs ${lpdev}p${idx}
				err_exit $?
				fsck.$type -y ${lpdev}p${idx}
				err_exit $?
			fi
		fi
	done

	losetup -d $lpdev
	sgdisk -p sdcard.img
}

if [ x"$1" == x"build" ];then
	build
	#mk_sdcard
else
	qemu-system-arm -M vexpress-a9 -m 1024M -smp 1 -nographic -kernel u-boot -sd ./sdcard.img 
	#qemu-system-arm -M vexpress-a9 -m 1024M -smp 1 -nographic -kernel u-boot -sd ./sdcard.img -netdev user,id=net0 -device virtio-net-device,netdev=net0 
    # qemu-system-aarch64 -machine virt -cpu cortex-a57 -m 2G -netdev user,id=net0 -device virtio-net-device,netdev=net0 -bios u-boot.bin -drive file=emmc.img,if=none,id=drive0 -device virtio-blk-device,drive=drive0 -nographic

fi
 
