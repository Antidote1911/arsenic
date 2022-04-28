#!/usr/bin/env bash

git tag v5.0 -m "Version 5.0"

echo "Pushing Local Files To Github..."
git push origin v5.0

echo "Git Push Completed!"
