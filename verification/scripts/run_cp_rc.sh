#/bin/sh
host_ip=$1

# run CP application on terminal
sshpass -p ezchip ssh -t -t root@$host_ip <<EOF
/tmp/cp_app
EOF
