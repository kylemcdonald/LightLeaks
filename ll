#!/usr/bin/env bash

REMOTE_DIR="admin@lightleaks.local:~/lightleaks"

function usage() {
    cat << EOF
./ll
  
  Controls the Light Leaks data.

  help: print this message
  app push/pull: push/pull the app from this computer to the remote computer
  mask push/pull: push/pull the mask from this computer to the remote computer
  data push/pull: push/pull the SharedData from this computer to the remote computer
EOF
}

function invalid() {
    echo "Invalid command"
    usage
}

case "$1" in
help)
    usage
    ;;
app)
    case "$2" in
        push)
            rsync -avz  4-LightLeaks $REMOTE_DIR/4-LightLeaks
        ;;
        pull)
            rsync -avz  $REMOTE_DIR/4-LightLeaks 4-LightLeaks
        ;;
        *)
            invalid
        ;;
mask)
    case "$2" in
        push)
            rsync -avz  SharedData/mask-0.png $REMOTE_DIR/SharedData
        ;;
        pull)
            rsync -avz  $REMOTE_DIR/SharedData/mask-0.png SharedData
        ;;
        *)
            invalid
        ;;
data)
    case "$2" in
        push)
            rsync -avz SharedData/ $REMOTE_DIR/SharedData
        ;;
        pull)
            rsync -avz $REMOTE_DIR/SharedData/ SharedData
        ;;
        *)
            invalid
        ;;
    esac
    ;;
*)
    invalid
    ;;
esac