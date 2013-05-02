#!/bin/bash

OUT_FILE=$1
jsonmod -p scm -v git -i $OUT_FILE
url=$(git config --get remote.origin.url)
jsonmod -p url -v $url -i $OUT_FILE
branch=$(basename $(git symbolic-ref HEAD))
jsonmod -p branch -v $branch -i $OUT_FILE
remote=$(git config --get "branch.$branch.remote")
jsonmod -p remote -v $remote -i $OUT_FILE
remote_branch=$(git config --get "branch.$branch.merge")
if [ ! -z $remote_branch ]; then
    remote_branch=$(basename $remote_branch)
    jsonmod -p remote_branch -v $remote_branch -i $OUT_FILE
fi
if [ -z $remote ] || [ -z $remote_branch ]; then
    echo "Could not find remote branch for ***$project_name***, using local HEAD as sha!"
    common_ancestor=$(git rev-parse HEAD)
else
    common_ancestor=$(git merge-base HEAD $remote/$remote_branch)
fi
jsonmod -p common_ancestor -v $common_ancestor -i $OUT_FILE

