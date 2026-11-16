#!/bin/sh
set -eu

# Given: a small, repeatable comparison; no plotting or report generator needed.
tasks=${1:-20000}

echo "backend,agents,tasks,elapsed_ms,tasks_per_second,checksum"
for agents in 1 2 4; do
    for backend in mutex mq shm; do
        ./agentlab "$backend" "$agents" "$tasks"
    done
done
