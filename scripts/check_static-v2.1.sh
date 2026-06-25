#!/bin/bash -

PATH_PREFIX=/home/PDC-PSW/workspace/tina
[ -d $PWD ] && [ -f rules.mk ] && PATH_PREFIX=$PWD
CPPCHECK=${PATH_PREFIX}/tools/cppcheck/cppcheck-v2.0
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:./tools/cppcheck/"
check_path="."
if [ "$1" ] ;then
	check_path=$1
fi

OUT_DIR=out/check_static_v2_1/result
mkdir -p "${OUT_DIR}"

CHECK_LOG="${OUT_DIR}/static.log"
CHECK_LOG_SORT="${OUT_DIR}/static_sort.log"
CHECK_ERR="${OUT_DIR}/static-err.log"
CHECK_WARN="${OUT_DIR}/static-warn.log"
CHECK_TMP_LOG="${OUT_DIR}/tmp_static.log"

rm -f ${CHECK_LOG} ${CHECK_ERR} ${CHECK_WARN}

echo check: "$check_path"
echo out log: "$OUT_DIR"
src_list=$check_path

function do_check()
{
${CPPCHECK} -j32 --enable=warning --force -x c++ \
	--suppress=nullPointerRedundantCheck \
	--suppress=shiftTooManyBitsSigned \
	--suppress=preprocessorErrorDirective \
	--suppress=syntaxError \
	--suppress=invalidPrintfArgType_sint \
	--suppress=invalidScanfArgType_int \
	--template '{file},{line},{severity},{id},{message}' \
    $1 \
	--output-file=${CHECK_TMP_LOG}
}

while read dpath
do
    [ -z ${dpath} ] && continue;
    [ -d ${PATH_PREFIX}/${dpath} ] || continue;

    #echo "per line:${dpath}..."
    do_check ${PATH_PREFIX}/$dpath
    cat ${CHECK_TMP_LOG} >> ${CHECK_LOG}
    rm -f ${CHECK_TMP_LOG} 2> /dev/null
done < ${src_list}

# avoid that there is no check log
touch ${CHECK_LOG} ${CHECK_ERR} ${CHECK_WARN}
rm ${CHECK_LOG_SORT}
sort ${CHECK_LOG} > ${CHECK_LOG_SORT}
# parse result
awk -F, '$3~/warning/{print}' ${CHECK_LOG_SORT} > ${CHECK_WARN} 2>/dev/null
awk -F, '$3~/error/{print}' ${CHECK_LOG_SORT} > ${CHECK_ERR} 2>/dev/null
WARN_CNT=`wc -l < ${CHECK_WARN}`
ERR_CNT=`wc -l < ${CHECK_ERR}`
echo
echo ERROR Count : ${ERR_CNT}
echo WARNING Count : ${WARN_CNT}

tree ${OUT_DIR}
