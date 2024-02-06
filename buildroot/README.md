编译时删除本地的libssl1.0-dev,安装 libssl-dev
boot.cmd中更改挂载设备类型为dm-verity以便avb验证：
dm="dm-mod.create=\"system,,,ro, 0 $part_size linear /dev/mmcblk0p${part_number} 0\""

linear 更改为 verity