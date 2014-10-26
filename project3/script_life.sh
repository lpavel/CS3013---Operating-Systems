echo "Script that runs the"
echo "./life 6 gen0.in 120 n n > answer_bigger_matrix.txt"
./life 6 gen0.in 120 n n > answer_bigger_matrix.txt

echo "./life 6 gen0-small1.in 40 y n > small1_matrix.txt"
./life 6 gen0-small1.in 40 y n > small1_matrix.txt

echo "./life 8 gen0-small6.in 200 n n > small6_matrix.txt"
./life 8 gen0-small6.in 120 n n > small6_matrix.txt

echo "./life 6 gen0-small7.in 120 n n > small7_matrix.txt"
./life 6 gen0-small7.in 120 n n > small7_matrix.txt

echo "./life 6 gen0-too_many_columns.in 57 n n"
./life 4 gen0-too_many_columns.in 57

echo "./life 6 gen0-too_many_lines.in 57 n n"
./life 6 gen0-too_many_lines.in 57

echo "./life 6 gen0-max_number_lines.in 19 y > max_lines_matrix.txt"
./life 6 gen0-max_number_lines.in 19 y > max_lines_matrix.txt

echo "./life 6 gen0-max_number_columns.in 43 y > max_columns_matrix.txt"
./life 6 gen0-max_number_columns.in 43 y > max_columns_matrix.txt

echo "./life 6 gen0-alldie.in 43 y > alldie.txt"
./life 6 gen0-alldie.in 43 y > alldie.txt
