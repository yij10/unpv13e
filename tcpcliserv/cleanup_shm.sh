#!/bin/bash

# 列出所有共享記憶體段
ipcs -m | awk 'NR>3 {if ($6 == 0) print $2}' | while read shmid; do
    echo "Deleting unused shared memory segment ID: $shmid"
    ipcrm -m "$shmid"
done

echo "Cleanup completed."
