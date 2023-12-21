# cpp
cpp learning 

## for display (host only adapter)
* install vcxsrv for windows
* apt install x11-apps
* export DISPLAY=192.168.56.1:0.0
* xeyes

## for audio
* download pulseaudio for windows

* apt install -y bash pulseaudio alsa-utils sox libsox-fmt-all

* export PULSE_SERVER=192.168.56.1
* load-module module-native-protocol-tcp listen=0.0.0.0 auth-anonymous=1
* pulseaudio.exe --use-pid-file=false

# gst debug 
* export GST_DEBUG=2

## use chrome as gest and bypass cert to add Shortcut Properties target
* --guest  --test-type --origin-to-force-quic-on=localhost:8500 --ignore-certificate-errors-spki-list=2RtYQI8GxZ4WGEsnQaggTyRmca1dP5SY3Qfy+Mj+WMM=
