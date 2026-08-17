3
BEGIN
 DUP 0 >
WHILE  
   'A' . CR
   1 -
REPEAT DROP
'B' . CR CR

3
BEGIN
   'A' . CR
   1 -
   DUP 0 =
UNTIL
DROP
'B' . CR CR

3 0 DO
   'A' . CR
LOOP
'B' . CR CR

10 0 DO
   1 INDEX . CR
LOOP
CR CR

3 0 DO
   2 0 DO
    '(' . 2 INDEX . ',' .  1 INDEX . ')' . CR
   LOOP
LOOP
CR CR

3 2 > IF
   'T' 'F' = IF
      'A'
   ELSE
      'B'
   THEN
ELSE
   'F' 'F' 0= IF
      'C'
   ELSE
      'D'
   THEN
THEN
. CR
true IF 1 THEN
