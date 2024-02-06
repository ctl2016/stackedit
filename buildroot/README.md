1. 编译时删除本地的libssl1.0-dev,安装 libssl-dev
2. boot.cmd中更改挂载设备类型为dm-verity以便avb验证：
dm="dm-mod.create=\"system,,,ro, 0 $part_size linear /dev/mmcblk0p${part_number} 0\""

linear 更改为 verity

3.创建key

openssl genpkey -algorithm RSA -out private_key.pem -pkeyopt rsa_keygen_bits:4096
openssl rsa -in private_key.pem -pubout -out public_key.pem