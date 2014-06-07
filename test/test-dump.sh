#! /bin/bash
PARSER=../src/dump
TMPFILE=`mktemp`

FAILURES=0

SUFFIX=$1
shift 1

for TEST in `cat TESTS`; do
    printf "%-30s" $TEST
    # The sed command below replaces built-in symbol identifiers by a single `-', to ensure that we don't need to
    # change the expected test results every time we change the list of builtins-- builtins should be unique by name already.
    ${PARSER} $@ < ${TEST}.av | sed 's/SYM\[-.*\]/SYM[-]/' > ${TEST}.${SUFFIX}.last
    if diff -u ${TEST}.${SUFFIX}.last ${TEST}.${SUFFIX} > ${TMPFILE}; then
	printf "\033[032;1mOK\033[0m\n"
    else
	FAILURES=$((FAILURES + 1))
	printf "\033[031;1mFAILURE\033[0m (%s vs %s)\n" ${TEST}.${SUFFIX}.last ${TEST}.${SUFFIX}
	diff  ${TEST}.${SUFFIX} ${TEST}.${SUFFIX}.last
    fi
done
if [ ${FAILURES} -gt 0 ]; then
    printf "%s failures: %d\n" ${SUFFIX} ${FAILURES}
fi

rm -f ${TMPFILE}

exit ${FAILURES}
