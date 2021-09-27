#sometimes ccache messes up on the zoo, and this needs to be uncommented
#export CC="/usr/bin/gcc"

echo "Building submit"
bazel build //submit:all
echo "Building results"
bazel build //results_view:all

echo "Copying submit"

cp -v bazel-bin/submit/submit-1 /c/cs323/proj1/submit
cp -v bazel-bin/submit/submit-obj2exec /c/cs323/proj2/submit-obj2exec
cp -v bazel-bin/submit/submit-dmalloc /c/cs323/proj2/submit-dmalloc
cp -v bazel-bin/submit/submit-4 /c/cs323/proj4/submit

chmod g+rws /c/cs323/proj1/submit
chmod g+rws /c/cs323/proj2/submit-obj2exec
chmod g+rws /c/cs323/proj2/submit-dmalloc
chmod g+rws /c/cs323/proj4/submit

chmod u+rwx /c/cs323/proj1/submit
chmod u+rwx /c/cs323/proj2/submit-obj2exec
chmod u+rwx /c/cs323/proj2/submit-dmalloc
chmod u+rwx /c/cs323/proj4/submit


echo "Copying results"
cp -v bazel-bin/results_view/results-1 /c/cs323/proj1/results
cp -v bazel-bin/results_view/results-2 /c/cs323/proj2/results
cp -v bazel-bin/results_view/results-4 /c/cs323/proj4/results

chmod g+rws /c/cs323/proj1/results
chmod g+rws /c/cs323/proj2/results
chmod g+rws /c/cs323/proj4/results

chmod u+rwx /c/cs323/proj1/results
chmod u+rwx /c/cs323/proj2/results
chmod u+rwx /c/cs323/proj4/results
