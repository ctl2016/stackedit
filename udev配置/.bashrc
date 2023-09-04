# usb device manager

if [ -z `pidof systemd-udevd` ];then
    echo '123456' | sudo -S /lib/systemd/systemd-udevd --daemon > /dev/null 2>&1
fi

