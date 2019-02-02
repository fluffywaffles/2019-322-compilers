#!/bin/zsh

zmodload zsh/datetime

start_time=$EPOCHREALTIME
outputString=`./bin/L2 -g 0 -l 1 -O0 $@` ;
end_time=$EPOCHREALTIME
printf "compile   : $((end_time - start_time))\n" 1>&2

start_time=$EPOCHREALTIME
while read -r line ; do
  lineWithoutPrefix=`echo "$line" | sed "s/^(//"` ;
  lineWithoutPrefixAndSuffix=`echo "$lineWithoutPrefix" | sed "s/)//"` ;

  if test "$lineWithoutPrefix" = "" ; then
    echo $line ;
    continue ;
  fi
  if test "$line" = "$lineWithoutPrefix" -o "$lineWithoutPrefixAndSuffix" = "$lineWithoutPrefix" ; then
    echo $line ;
    continue ;
  fi

  echo -n "(";
  lineToPrint=`echo "$lineWithoutPrefixAndSuffix" | xargs -n1 | sort -u | xargs` ;
  echo "$lineToPrint)";
done <<< "$outputString" ;
end_time=$EPOCHREALTIME
printf "normalize : $((end_time - start_time))\n" 1>&2
