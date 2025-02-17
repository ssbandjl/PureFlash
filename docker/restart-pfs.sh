#!/bin/bash
set -m

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

JAVA_HOME=/usr/lib/jvm/jdk-15/
export PATH=/opt/pureflash:$JAVA_HOME/bin:$PATH


OLD_PID=$(pidof pfs)
if [ "$OLD_PID" != "" ]; then
	echo "OLD_PID:$OLD_PID "
	kill -2 $OLD_PID
fi

echo "Restart PureFlash store..."
nohup $DIR/pfs -c /etc/pureflash/pfs.conf > /var/log/pfs.log 2>&1 &
status=$?
if [ $status -ne 0 ]; then
  echo "Failed to start pfs: $status"
  exit $status
fi

