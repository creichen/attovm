#! /bin/bash
# Usage:
#   localizescript.sh version file1 file2 ...

version=$1
shift 1
filelist=$@
dist_name=attovm-${version}

mkdir -p ./en/${dist_name}
cp --parents ${filelist} "./en/${dist_name}";
cp -rf ./en './de/';

echo 'Attempting to create german distribution';
cd de;
echo 'Removing multi line english comments from german distribution';
perl -0777 -i -pe 's{/\*[\*]*e.*?\*/}{}gs'  `find ./ -name '*.c'`;
perl -0777 -i -pe 's{/\*[\*]*e.*?\*/}{}gs'  `find ./ -name '*.h'`;
echo 'Removing single line english comments from german distribution';
sed -i "/\/\/e/d" `find ./ -name '*.c'`;
sed -i "/\/\/e/d" `find ./ -name '*.h'`;
cd ..;

echo 'Attempting to create english distribution';
cd en;
echo 'Removing multi line german comments from english distribution';
perl -0777 -i -pe 's{/\*[\*]*d.*?\*/}{}gs'  `find ./ -name '*.c'`;
perl -0777 -i -pe 's{/\*[\*]*d.*?\*/}{}gs'  `find ./ -name '*.h'`;
echo 'Removing single line german comments from english distribution';
sed -i "/\/\/d/d" `find ./ -name '*.c'`;
sed -i "/\/\/d/d" `find ./ -name '*.h'`;
cd ..;
echo 'Creating extractions';
cd de;
tar -zcvf attovm-de-${version}.tar.gz ${dist_name}
cd ..
cd en;
tar -zcvf attovm-en-${version}.tar.gz ${dist_name}
cd ..
cp de/*.tar.gz .
cp en/*.tar.gz .
rm -rf de;
rm -rf en;


