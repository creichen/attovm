cp -rf ./ './en/';

cp -rf ./en './de/';

echo 'Attempting to create german distribution';
cd de;
echo 'Removing multi line english comments from german distribution';
perl -0777 -i -pe 's{/\*[\*]*e.*?\*/}{}gs'  `find ./ -name '*.c'`;
perl -0777 -i -pe 's{/\*[\*]*e.*?\*/}{}gs'  `find ./ -name '*.h'`;
echo'Removing single line english comments from german distribution';
sed -i "/\/\/e/d" `find ./ -name '*.c'`;
sed -i "/\/\/e/d" `find ./ -name '*.h'`;
cd ..;

echo'Attempting to create english distribution';
cd en;
echo 'Removing multi line german comments from english distribution';
perl -0777 -i -pe 's{/\*[\*]*d.*?\*/}{}gs'  `find ./ -name '*.c'`;
perl -0777 -i -pe 's{/\*[\*]*d.*?\*/}{}gs'  `find ./ -name '*.h'`;
echo 'Removing single line german comments from english distribution';
sed -i "/\/\/d/d" `find ./ -name '*.c'`;
sed -i "/\/\/d/d" `find ./ -name '*.h'`;
cd ..;
echo 'Creating extractions';
tar -zcvf attovm-de.tar.gz de;
tar -zcvf attovm-en.tar.gz en;
rm -rf de;
rm -rf en;


