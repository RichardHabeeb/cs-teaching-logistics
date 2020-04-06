
echo "Building submit"
bazel build //submit:all
echo "Building results"
bazel build //results_view:all

echo "Copying submit"

cp -v bazel-bin/submit/submit-1 /c/cs323/Hwk1/submit
cp -v bazel-bin/submit/submit-2 /c/cs323/Hwk2/submit
cp -v bazel-bin/submit/submit-4 /c/cs323/Hwk4/submit
cp -v bazel-bin/submit/submit-5 /c/cs323/Hwk5/submit
chmod g+rws /c/cs323/Hwk1/submit
chmod g+rws /c/cs323/Hwk2/submit
chmod g+rws /c/cs323/Hwk4/submit
chmod g+rws /c/cs323/Hwk5/submit


echo "Copying results"
cp -v bazel-bin/results_view/results-1 /c/cs323/Hwk1/results
cp -v bazel-bin/results_view/results-2 /c/cs323/Hwk2/results
cp -v bazel-bin/results_view/results-4 /c/cs323/Hwk4/results
cp -v bazel-bin/results_view/results-5 /c/cs323/Hwk5/results
chmod g+rws /c/cs323/Hwk1/results
chmod g+rws /c/cs323/Hwk2/results
chmod g+rws /c/cs323/Hwk4/results
chmod g+rws /c/cs323/Hwk5/results
