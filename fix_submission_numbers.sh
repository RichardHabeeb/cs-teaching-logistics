#Will fill in gaps in submission numbers.

for netid in $1/*; do
  a=0
  for i in $netid/*; do
    new=$(printf "$netid/%d" "$a")
    echo "MOVING: $i $new"
    [[ "$i" == "$new" ]] || mv -v -i -- "$i" "$new"
    let a=a+1
  done
done
