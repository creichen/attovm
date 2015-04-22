.text
main:
        push $fp      ; align stack
        move $fp, $sp

	la   $a0, hello
        jal  print_string  ; print out
        ld   $a0, number($gp)
        jal  print_int
        la   $t0, more_numbers
        ld   $v0, 0($t0)
        ld   $a0, 8($t0)
        add  $a0, $v0
        jal  print_int     ; print sum

        pop  $fp
	jreturn
.data
hello:
.asciiz "Hello, World!"
.word
number:
	23
more_numbers:
	3,4
