This is the library of dependability aspects.
These aspects are software modules that can be compiled
with the AspectC++ 2.x compiler to augment any C/C++ software by
transparent error-detection and error-correction mechanisms.
The library contains the following software modules:

 * ArrayBoundsCheck: Check for out-of-bounds array access
 * FunctionPointerCheck: Check for invalid function pointers
 * GenericObjectProtection: Extend classes/structs by error-correcting codes
 * IntegerOverflowCheck: Detect integer-overflow errors
 * ReturnAddressProtection: Detect and correct errors in function return addresses
 * TypeCheck: Apply lightweight run-time type checking
 * VirtualFunctionProtection: Detect and correct errors in virtual function pointers

Each subfolder contains a UNIX-style Makefile to compile an example program with
such a dependability aspect.

For more information, please refer to http://dx.doi.org/10.17877/DE290R-17995

