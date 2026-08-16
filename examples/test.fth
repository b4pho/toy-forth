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
