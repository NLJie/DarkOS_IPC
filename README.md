  bash build.sh && \
  sshpass -p "temppwd" scp build/products/cam_basic/cam_basic cat@192.168.1.110:/home/cat/mpp/ && \
  sshpass -p "temppwd" ssh cat@192.168.1.110 "cd /home/cat/mpp && ./cam_basic --config device_rk3588s_ov8858.json"