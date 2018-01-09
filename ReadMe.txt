
	Almost as soon as your application becomes something
more than "Hello World", you find yourself in need of a
means to persistently store configuration settings or other
data. The registry is generally a bad place for more than a
few settings because of it's limited size and difficulty of
use.  The common method is to use some form of configuration
file. 

  CDataFile is a C++ object who's purpose is to aid you in
reading and writing to configuration files.  It generally
tries to be compatible with standard Windows .ini files,
although it has some features that, if used, will break that
compatiblity. (namely non-standard comment indicators and
quoted key/value values).

  While I'm not a huge fan of std::string (and honestly, not
all that familiar with it), I felt that it would be the best
choice here. It's certainly portable and does not rely on
MFC or any other third party libraries. The lack of features
in std::string made me nearly regret that choice early on.

  Ample documentation for the class exists within the source
code and the provided test project.

Primary Features List:

CDataFile
#	Platform independant
#       No reliance on windows system calls.  You should
        be able to use this code, with minor modifications, 
	in your linux applications as well as your windows.
#	Support for multi-line comments for both sections 
	and keys
#	Multiple section support. This allows you to have
	duplicate keys, with possibly differing values (or
	the same for that matter)
#	Full string support allows you to have key names
	or values which contain spaces.
#	Easily configurable to your tastes by changing
	simple, well documented flags.
#	Simple error/message reporting functionality.
#	Can read and write Windows .ini files. 
#	Small, concise and wll commented.
