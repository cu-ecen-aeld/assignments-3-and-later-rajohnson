#! /bin/sh

case "$1" in
	start)
		echo "Starting aesdsocket"
		start-stop-daemon --start -n aesdsocket --exec /usr/bin/aesdsocket -- -d
		;;
	stop)
		echo "Stopping aesdsocket"
		start-stop-daemon -K -n aesdsocket
		;;
	*)
		echo "Usage: $0 {start|stop}"
	exit 1
esac

exit 0

