rm -f /opt/pureflash/data0.img && truncate  -s 10G /opt/pureflash/data0.img && losetup /dev/loop5 /opt/pureflash/data0.img
# 查看: losetup -a
# losetup --detach /dev/loop5


# cd /root/project/PureFlash/thirdParty/zookeeper
# ./bin/zkServer.sh start-foreground


export LD_LIBRARY_PATH=/root/project/PureFlash/build_deb/bin:$LD_LIBRARY_PATH:/root/project/PureFlash/thirdParty/spdk/dpdk/build/lib
cd /root/project/PureFlash/build_deb/bin
./pfs -c /root/project/PureFlash/build_deb/pfs.conf

gdb --args ./pfs  -c /root/project/PureFlash/build_deb/pfs.conf
