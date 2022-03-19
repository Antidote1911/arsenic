#!/usr/bin/env bash

## Push File To Repository

echo "Checking For Newer Files Online..."
git pull

echo "Backing Up Everything In Project Folder..."
git add --all . | tee -a README.md

read -p "Enter The Commit Message : " input

echo "Committing To The Local Repository..."
git commit -m "$input"

echo "Pushing Local Files To Github..."
git push

echo "Git Push Completed!"
