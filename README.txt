Project 3 - Network Communications

Group Members:
- Gaurav Patel (gvp23)
- Pradhyum Krishnan (pk555)

***NOTE***
- Use nc to test the code; since the professor said in lecture that nc is sufficient, we haven't made a client file. Thus, ONLY use nc to the code. To close the client side connection in nc, use Control + C. Please note, after an error, the connection is closed, so you will have to use Control + C to exit (you cannot type any commmands after an error). Thank you! 

Testing Strategy:
- To start of the project, we first layed out all of the possible cases we would encounter; for instance, we discussed how we would read in the the data passed in by the client. Since using read would only get certain amount of bytes at a time, we decided to forward all of the input into a file and then read it byte by byte using getc; this ensured that all of the data was read in correcrly. After this, we started laying out all of the ways we would check for errors. Therefore, we devised multiple test cases with wrong length, bad format, and words than contained null-terminators. Throughout the testng, we had memory leak issues; howeover, we solved them by running through our code step by step. When testing, we also tested for scenarios where there was no input, invalid phrases, valid inputs with all commands, and more. We had also tried to use a variation of sequences of commands to verify our program functions to expectations. Moreover, we also used multiple clients (used "nc" on multiple devices at the same time) to ensure that our code worked with differents connections and ensured synchronization throughout the program. 
