suchetzky, eliamerran
Tal Suchetzky (208562983), Elia Merran (209422831)
EX: 2

FILES:


REMARKS:
-

ANSWERS:

Assigment 1:

1.
	a.1
		int sigsetjmp(sigjmp_buf env, int savemask) - This function saves the current
		context of the program, saves the current stack environment including, optionally,
		the current signal mask.
		The stack environment and signal mask saved by sigsetjmp() can subsequently
		be restored by siglongjmp().

		return value:
		Returns 0 when it is invoked to save the stack environment and signal mask.

		sigsetjmp() returns the value val, specified on siglongjmp() (or 1 if the value of val is zero),
		when siglongjmp() causes control to be transferred to the place in the user's
		program where sigsetjmp() was issued.

	a.2 
		void siglongjmp( sigjmp_buf env, int val ) - The siglongjmp() function restores
		the stack environment previously saved in env by sigsetjmp().
		siglongjmp() also provides the option to restore the signal mask,
		depending on whether the signal mask was saved by sigsetjmp().
		
		return value: none 
	

	b.
		sigsetjmp can save the current signal mask. 
		siglongjmp() provides the option to restore the signal mask,
		depending on whether the signal mask was saved by sigsetjmp(). 
		
2. 