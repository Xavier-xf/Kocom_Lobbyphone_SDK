#!/bin/bash 
get_version_info()
{
    local LOCAL_PATH=`pwd`
    cd $1
    local REMOTE_REPO_TAG="`git for-each-ref --sort=taggerdate --format '%(refname)' | tail -n 1 | grep  \"refs/tags/\"`"
    local REPO_TAG=`echo "${REMOTE_REPO_TAG}" | awk -F "refs/tags/" '{print $2}'`

    local REMOTE_REPO_BRANCH="` git branch | grep \* `"
    local REPO_BRANCH=`echo "${REMOTE_REPO_BRANCH}" | awk '{print $2}'`

    local REMOTE_REPO_COMMIT="` git log -1 | grep \"^commit \"`"

    local REPO_COMMIT=`echo "${REMOTE_REPO_COMMIT}" | awk '{print $2}'`

    local REMOTE_REPO_DATE="` git log -1 | grep \"^Date:   \"`"
    local REPO_DATE="${REMOTE_REPO_DATE#"Date:   "}"

    local REMOTE_REPO_AUTHOR="` git log -1 | grep "^Author:"`"
    local REPO_AUTHOR="` echo ${REMOTE_REPO_AUTHOR} | cut -d ' ' -f 2`"

    local REMOTE_REPO_CHANGE_ID="$( git log -1 | grep "Change-Id:")"
    local REPO_CHANGE_ID="$( echo ${REMOTE_REPO_CHANGE_ID} | cut -d ' ' -f 2)"

    local RELEASE_AUTHOR=`whoami`

    echo "--${REPO_TAG} : ${REPO_BRANCH} : ${REPO_COMMIT} : ${REPO_DATE} : ${REPO_AUTHOR} : ${REPO_CHANGE_ID} : ${RELEASE_AUTHOR}----"

    sed -i "/^#define REPO_TAG/c#define REPO_TAG \"${REPO_TAG}\"" ./cdc_version.h
	sed -i "/^#define REPO_BRANCH/c#define REPO_BRANCH \"${REPO_BRANCH}\"" ./cdc_version.h
	sed -i "/^#define REPO_COMMIT/c#define REPO_COMMIT \"${REPO_COMMIT}\"" ./cdc_version.h
    sed -i "/^#define REPO_DATE/c#define REPO_DATE \"${REPO_DATE}\"" ./cdc_version.h
    sed -i "/^#define REPO_AUTHOR/c#define REPO_AUTHOR \"${REPO_AUTHOR}\"" ./cdc_version.h
    sed -i "/^#define REPO_CHANGE_ID/c#define REPO_CHANGE_ID \"${REPO_CHANGE_ID}\"" ./cdc_version.h
    sed -i "/^#define RELEASE_AUTHOR/c#define RELEASE_AUTHOR \"${RELEASE_AUTHOR}\"" ./cdc_version.h
    cd ${LOCAL_PATH}
}
get_version_info $1
