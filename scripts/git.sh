#!/usr/bin/env bash

set -e

GIT_NAME="$1"
GIT_EMAIL="$2"
COMMIT_MSG="$3"

if [[ -z "$GIT_NAME" || -z "$GIT_EMAIL" || -z "$COMMIT_MSG" ]]; then
	echo "Usage: $0 \"<user.name>\" \"<user.email>\" \">commit_message>\""
	exit 1
fi

git config --global user.name "$GIT_NAME"
git config --global user.emal "$GIT_EMAIL"
git add .
git commit -m "$COMMIT_MSG"
git push
