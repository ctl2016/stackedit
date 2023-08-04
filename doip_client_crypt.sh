! #/bin/bash
CFLAGS=-static shc -r -f doip_client.sh
upx -9 doip_client.sh.x
mv doip_client.sh.x doip_client
