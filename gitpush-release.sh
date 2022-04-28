#!/usr/bin/env bash

## Push File To Repository

echo "Checking For Newer Files Online..."
git pull

echo "Backing Up Everything In Project Folder..."
git add --all . | tee -a README.md

read -p "Enter The Commit Message : " input

echo "Committing To The Local Repository..."
git commit -m "$input"

git tag 5.0.0

echo "Pushing Local Files To Github..."
git push origin 5.0.0

echo "Git Push Completed!"
