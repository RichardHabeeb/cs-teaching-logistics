echo "MAKE DRY RUN: check for safety"
make -n
read -p "Press enter to continue."
/c/cs323/Hwk1/.final.pl > TEST_RUN_OUTPUT 2>&1
cat TEST_RUN_OUTPUT
