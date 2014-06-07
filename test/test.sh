#/bin/bash
ERRORS=0

echo "Parser"
echo "======"
./test-dump.sh parse -r
ERRORS=$((ERRORS + $?))
echo ""

echo "Name analysis"
echo "============="
./test-dump.sh names -a
ERRORS=$((ERRORS + $?))
echo ""

echo "Symbol table"
echo "============"
./test-dump.sh symtab -s
ERRORS=$((ERRORS + $?))
echo ""

if [ ${ERRORS} -gt 0 ]; then
    printf "TOTAL errors: %d\n" ${ERRORS}
fi

